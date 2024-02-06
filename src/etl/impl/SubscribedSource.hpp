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

#pragma once

#include "etl/ETLHelpers.hpp"
#include "feed/SubscriptionManager.hpp"
#include "util/Mutex.hpp"
#include "util/Retry.hpp"
#include "util/log/Logger.hpp"
#include "util/requests/Types.hpp"
#include "util/requests/WsConnection.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace etl::impl {

class SubscribedSource {
public:
    using OnDisconnectHook = std::function<void()>;

private:
    util::Logger log_;
    util::requests::WsConnectionBuilder wsConnectionBuilder_;

    struct ValidatedLedgersData {
        std::vector<std::pair<uint32_t, uint32_t>> validatedLedgers;
        std::string validatedLedgersRaw{"N/A"};
    };
    util::Mutex<ValidatedLedgersData> validatedLedgersData_;

    std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers_;

    std::shared_ptr<feed::SubscriptionManager> subscriptions_;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    util::Retry retry_;

    OnDisconnectHook onDisconnect_;

    std::atomic_bool isConnected_{false};
    std::atomic_bool stop_{false};
    std::atomic_bool isForwarding_{false};

    util::Mutex<std::chrono::system_clock::time_point> lastMessageTime_;

    static constexpr std::chrono::seconds CONNECTION_TIMEOUT{30};
    static constexpr std::chrono::seconds RETRY_MAX_DELAY{30};
    static constexpr std::chrono::seconds RETRY_DELAY{1};

public:
    SubscribedSource(
        boost::asio::io_context& ioContext,
        std::string const& ip,
        std::string const& wsPort,
        std::shared_ptr<NetworkValidatedLedgers> validatedLedgers,
        std::shared_ptr<feed::SubscriptionManager> subscriptions,
        OnDisconnectHook onDisconnect
    );

    bool
    hasLedger(uint32_t sequence) const;

    bool
    isConnected() const;

    void
    setForwarding(bool isForwarding);

    std::chrono::system_clock::time_point
    lastMessageTime() const;

private:
    void
    subscribe();

    std::optional<util::requests::RequestError>
    handleMessage(std::string const& message);

    void
    handleError(util::requests::RequestError const& error);

    void
    onConnect();

    void
    logError(util::requests::RequestError const& error) const;

    void
    setLastMessageTime();

    void
    setValidatedRange(std::string range);

    static std::string const&
    getSubscribeCommandJson();
};

}  // namespace etl::impl
