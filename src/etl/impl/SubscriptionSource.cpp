//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "etl/impl/SubscriptionSource.hpp"

#include "util/Expected.hpp"
#include "util/Retry.hpp"
#include "util/log/Logger.hpp"
#include "util/requests/Types.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_to.hpp>
#include <boost/lexical_cast.hpp>
#include <fmt/core.h>
#include <openssl/err.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace etl::impl {

SubscriptionSource::~SubscriptionSource()
{
    std::cout << "destructor" << std::endl;
    stop();
    std::cout << "destructor: after stop" << std::endl;

    std::future<void> future = boost::asio::post(strand_, boost::asio::use_future);
    future.get();
    std::cout << "destructor: after wait" << std::endl;

    wsConnection_.reset();
}

bool
SubscriptionSource::hasLedger(uint32_t sequence) const
{
    auto validatedLedgersData = validatedLedgersData_.lock();
    for (auto& pair : validatedLedgersData->validatedLedgers) {
        if (sequence >= pair.first && sequence <= pair.second) {
            return true;
        }
        if (sequence < pair.first) {
            // validatedLedgers_ is a sorted list of disjoint ranges
            // if the sequence comes before this range, the sequence will
            // come before all subsequent ranges
            return false;
        }
    }
    return false;
}

bool
SubscriptionSource::isConnected() const
{
    return isConnected_;
}

void
SubscriptionSource::setForwarding(bool isForwarding)
{
    isForwarding_ = isForwarding;
}

std::chrono::system_clock::time_point
SubscriptionSource::lastMessageTime() const
{
    return lastMessageTime_.lock().get();
}

void
SubscriptionSource::stop()
{
    stop_ = true;
}

void
SubscriptionSource::subscribe()
{
    std::cout << "subscribe" << std::endl;
    boost::asio::spawn(strand_, [this, _ = boost::asio::make_work_guard(strand_)](boost::asio::yield_context yield) {
        std::cout << "connect" << std::endl;
        auto connection = wsConnectionBuilder_.connect(yield);
        if (not connection) {
            std::cout << "connect error" << std::endl;
            handleError(connection.error(), yield);
            std::cout << "connect error handle done" << std::endl;
            return;
        }

        std::cout << "write" << std::endl;
        auto const& subscribeCommand = getSubscribeCommandJson();
        auto const writeErrorOpt = connection.value()->write(subscribeCommand, yield);
        if (writeErrorOpt) {
            std::cout << "write error" << std::endl;
            handleError(writeErrorOpt.value(), yield);
            return;
        }

        onConnect();

        while (!stop_) {
            std::cout << "read" << std::endl;
            auto message = connection.value()->read(yield);
            if (not message) {
                std::cout << "read error" << std::endl;
                handleError(message.error(), yield);
                return;
            }

            std::cout << "handle" << std::endl;
            auto handleErrorOpt = handleMessage(message.value());
            if (handleErrorOpt) {
                std::cout << "handle error" << std::endl;
                handleError(handleErrorOpt.value(), yield);
                return;
            }
        }
    });
}

std::optional<util::requests::RequestError>
SubscriptionSource::handleMessage(std::string const& message)
{
    setLastMessageTime();

    try {
        auto const raw = boost::json::parse(message);
        auto const object = raw.as_object();
        uint32_t ledgerIndex = 0;

        // TODO move strings to constants
        if (object.contains("result")) {
            auto const& result = object.at("result").as_object();
            if (result.contains("ledger_index"))
                ledgerIndex = result.at("ledger_index").as_int64();

            if (result.contains("validated_ledgers")) {
                auto validatedLedgers = boost::json::value_to<std::string>(result.at("validated_ledgers"));
                setValidatedRange(std::move(validatedLedgers));
            }
            LOG(log_.info()) << "Received a message on ledger subscription stream. Message : " << object;

        } else if (object.contains("type") && object.at("type") == "ledgerClosed") {
            LOG(log_.info()) << "Received a message on ledger subscription stream. Message : " << object;
            if (object.contains("ledger_index")) {
                ledgerIndex = object.at("ledger_index").as_int64();
            }
            if (object.contains("validated_ledgers")) {
                auto validatedLedgers = boost::json::value_to<std::string>(object.at("validated_ledgers"));
                setValidatedRange(std::move(validatedLedgers));
            }

        } else {
            if (isForwarding_) {
                if (object.contains("transaction")) {
                    dependencies_.forwardProposedTransaction(object);
                } else if (object.contains("type") && object.at("type") == "validationReceived") {
                    dependencies_.forwardValidation(object);
                } else if (object.contains("type") && object.at("type") == "manifestReceived") {
                    dependencies_.forwardManifest(object);
                }
            }
        }

        if (ledgerIndex != 0) {
            LOG(log_.trace()) << "Pushing ledger sequence = " << ledgerIndex;
            dependencies_.pushValidatedLedger(ledgerIndex);
        }

        return std::nullopt;
    } catch (std::exception const& e) {
        LOG(log_.error()) << "Exception in handleMessage : " << e.what();
        return util::requests::RequestError{fmt::format("Error handling message: {}", e.what())};
    }
}

void
SubscriptionSource::handleError(util::requests::RequestError const& error, boost::asio::yield_context yield)
{
    std::cout << "inside handle error" << std::endl;
    isConnected_ = false;
    if (not stop_) {
        std::cout << "onDisconnect" << std::endl;
        onDisconnect_();
    }

    if (wsConnection_ != nullptr) {
        std::cout << "closing ws connection" << std::endl;
        auto const error = wsConnection_->close(yield);
        if (error) {
            LOG(log_.error()) << "Error closing websocket connection: " << error->message();
        }
        std::cout << "ws connection closed" << std::endl;
        wsConnection_.reset();
    }

    logError(error);
    if (not stop_) {
        std::cout << "schedule retry" << std::endl;
        retry_.retry([this] {
            std::cout << "retry" << std::endl;
            subscribe();
        });
    }
}

void
SubscriptionSource::onConnect()
{
    isConnected_ = true;
    retry_.reset();
}

void
SubscriptionSource::logError(util::requests::RequestError const& error) const
{
    if (error.errorCode() && error.errorCode()->category() == boost::asio::error::get_ssl_category()) {
        auto& errorCode = error.errorCode().value();
        std::string errorString = fmt::format(
            "({},{}) ",
            boost::lexical_cast<std::string>(ERR_GET_LIB(errorCode.value())),
            boost::lexical_cast<std::string>(ERR_GET_LIB(errorCode.value()))
        );

        static constexpr size_t BUFFER_SIZE = 128;
        char buf[BUFFER_SIZE];
        ::ERR_error_string_n(errorCode.value(), buf, sizeof(buf));
        errorString += buf;

        LOG(log_.error()) << errorString;
    }

    auto& errorCodeOpt = error.errorCode();
    // These are somewhat normal errors. operation_aborted occurs on shutdown,
    // when the timer is cancelled. connection_refused will occur repeatedly
    if (not errorCodeOpt or
        (errorCodeOpt.value() != boost::asio::error::operation_aborted &&
         errorCodeOpt.value() != boost::asio::error::connection_refused)) {
        LOG(log_.error()) << error.message();
    } else {
        LOG(log_.warn()) << error.message();
    }
}

void
SubscriptionSource::setLastMessageTime()
{
    lastMessageTime_.lock().get() = std::chrono::system_clock::now();
}

void
SubscriptionSource::setValidatedRange(std::string range)
{
    std::vector<std::string> ranges;
    boost::split(ranges, range, [](char const c) { return c == ','; });

    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    pairs.reserve(ranges.size());
    for (auto& pair : ranges) {
        std::vector<std::string> minAndMax;

        boost::split(minAndMax, pair, boost::is_any_of("-"));

        if (minAndMax.size() == 1) {
            uint32_t const sequence = std::stoll(minAndMax[0]);
            pairs.emplace_back(sequence, sequence);
        } else {
            if (minAndMax.size() != 2) {
                throw std::runtime_error(fmt::format(
                    "Error parsing range: {}.Min and max should be of size 2. Got size = {}", range, minAndMax.size()
                ));
            }
            uint32_t const min = std::stoll(minAndMax[0]);
            uint32_t const max = std::stoll(minAndMax[1]);
            pairs.emplace_back(min, max);
        }
    }
    std::sort(pairs.begin(), pairs.end(), [](auto left, auto right) { return left.first < right.first; });

    auto dataLock = validatedLedgersData_.lock();
    dataLock->validatedLedgers = std::move(pairs);
    dataLock->validatedLedgersRaw = std::move(range);
}

std::string const&
SubscriptionSource::getSubscribeCommandJson()
{
    static boost::json::object const jsonValue{
        {"command", "subscribe"},
        {"streams", {"ledger", "manifests", "validations", "transactions_proposed"}},
    };
    static std::string const jsonString = boost::json::serialize(jsonValue);
    return jsonString;
}

}  // namespace etl::impl
