//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "etl/LoadBalancer.hpp"

#include "data/BackendInterface.hpp"
#include "etl/ETLState.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/Source.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "rpc/Errors.hpp"
#include "util/Assert.hpp"
#include "util/Random.hpp"
#include "util/ResponseExpirationCache.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace util;
using namespace util::config;

namespace etl {

std::shared_ptr<LoadBalancer>
LoadBalancer::make_LoadBalancer(
    ClioConfigDefinition const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
    SourceFactory sourceFactory
)
{
    return std::make_shared<LoadBalancer>(
        config, ioc, std::move(backend), std::move(subscriptions), std::move(validatedLedgers), std::move(sourceFactory)
    );
}

LoadBalancer::LoadBalancer(
    ClioConfigDefinition const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
    SourceFactory sourceFactory
)
{
    auto const forwardingCacheTimeout = config.getValue("forwarding.cache_timeout").asFloat();
    if (forwardingCacheTimeout > 0.f) {
        forwardingCache_ = util::ResponseExpirationCache{
            util::config::ClioConfigDefinition::toMilliseconds(forwardingCacheTimeout),
            {"server_info", "server_state", "server_definitions", "fee", "ledger_closed"}
        };
    }

    static constexpr std::uint32_t MAX_DOWNLOAD = 256;
    auto const numMarkers = config.getValue("num_markers");
    if (numMarkers.hasValue()) {
        auto const value = numMarkers.asIntType<uint32_t>();
        ASSERT(value > 0 and value <= MAX_DOWNLOAD, "'num_markers' value in config must be in range 1-256");
        downloadRanges_ = value;
    } else if (backend->fetchLedgerRange()) {
        downloadRanges_ = 4;
    }

    auto const allowNoEtl = config.getValue("allow_no_etl").asBool();

    auto const checkOnETLFailure = [this, allowNoEtl](std::string const& log) {
        LOG(log_.warn()) << log;

        if (!allowNoEtl) {
            LOG(log_.error()) << "Set allow_no_etl as true in config to allow clio run without valid ETL sources.";
            throw std::logic_error("ETL configuration error.");
        }
    };

    auto const forwardingTimeout =
        ClioConfigDefinition::toMilliseconds(config.getValue("forwarding.request_timeout").asFloat());
    auto const etlArray = config.getArray("etl_sources");
    for (auto it = etlArray.begin<ObjectView>(); it != etlArray.end<ObjectView>(); ++it) {
        auto source = sourceFactory(
            *it,
            ioc,
            backend,
            subscriptions,
            validatedLedgers,
            forwardingTimeout,
            [this]() {
                if (not hasForwardingSource_.lock().get())
                    chooseForwardingSource();
            },
            [this](bool wasForwarding) {
                if (wasForwarding)
                    chooseForwardingSource();
            },
            [this]() {
                if (forwardingCache_.has_value())
                    forwardingCache_->invalidate();
            }
        );

        // checking etl node validity
        auto const stateOpt = ETLState::fetchETLStateFromSource(*source);

        if (!stateOpt) {
            LOG(log_.warn()) << "Failed to fetch ETL state from source = " << source->toString()
                             << " Please check the configuration and network";
        } else if (etlState_ && etlState_->networkID && stateOpt->networkID &&
                   etlState_->networkID != stateOpt->networkID) {
            checkOnETLFailure(fmt::format(
                "ETL sources must be on the same network. Source network id = {} does not match others network id = {}",
                *(stateOpt->networkID),
                *(etlState_->networkID)
            ));
        } else {
            etlState_ = stateOpt;
        }

        sources_.push_back(std::move(source));
        LOG(log_.info()) << "Added etl source - " << sources_.back()->toString();
    }

    if (!etlState_)
        checkOnETLFailure("Failed to fetch ETL state from any source. Please check the configuration and network");

    if (sources_.empty())
        checkOnETLFailure("No ETL sources configured. Please check the configuration");

    // This is made separate from source creation to prevent UB in case one of the sources will call
    // chooseForwardingSource while we are still filling the sources_ vector
    for (auto const& source : sources_) {
        source->run();
    }
}

LoadBalancer::~LoadBalancer()
{
    sources_.clear();
}

std::vector<std::string>
LoadBalancer::loadInitialLedger(uint32_t sequence, bool cacheOnly, std::chrono::steady_clock::duration retryAfter)
{
    std::vector<std::string> response;
    execute(
        [this, &response, &sequence, cacheOnly](auto& source) {
            auto [data, res] = source->loadInitialLedger(sequence, downloadRanges_, cacheOnly);

            if (!res) {
                LOG(log_.error()) << "Failed to download initial ledger."
                                  << " Sequence = " << sequence << " source = " << source->toString();
            } else {
                response = std::move(data);
            }

            return res;
        },
        sequence,
        retryAfter
    );
    return response;
}

LoadBalancer::OptionalGetLedgerResponseType
LoadBalancer::fetchLedger(
    uint32_t ledgerSequence,
    bool getObjects,
    bool getObjectNeighbors,
    std::chrono::steady_clock::duration retryAfter
)
{
    GetLedgerResponseType response;
    execute(
        [&response, ledgerSequence, getObjects, getObjectNeighbors, log = log_](auto& source) {
            auto [status, data] = source->fetchLedger(ledgerSequence, getObjects, getObjectNeighbors);
            response = std::move(data);
            if (status.ok() && response.validated()) {
                LOG(log.info()) << "Successfully fetched ledger = " << ledgerSequence
                                << " from source = " << source->toString();
                return true;
            }

            LOG(log.warn()) << "Could not fetch ledger " << ledgerSequence << ", Reply: " << response.DebugString()
                            << ", error_code: " << status.error_code() << ", error_msg: " << status.error_message()
                            << ", source = " << source->toString();
            return false;
        },
        ledgerSequence,
        retryAfter
    );
    return response;
}

std::expected<boost::json::object, rpc::ClioError>
LoadBalancer::forwardToRippled(
    boost::json::object const& request,
    std::optional<std::string> const& clientIp,
    bool isAdmin,
    boost::asio::yield_context yield
)
{
    if (not request.contains("command"))
        return std::unexpected{rpc::ClioError::rpcCOMMAND_IS_MISSING};

    auto const cmd = boost::json::value_to<std::string>(request.at("command"));
    if (forwardingCache_) {
        if (auto cachedResponse = forwardingCache_->get(cmd); cachedResponse) {
            return std::move(cachedResponse).value();
        }
    }

    ASSERT(not sources_.empty(), "ETL sources must be configured to forward requests.");
    std::size_t sourceIdx = util::Random::uniform(0ul, sources_.size() - 1);

    auto numAttempts = 0u;

    auto xUserValue = isAdmin ? ADMIN_FORWARDING_X_USER_VALUE : USER_FORWARDING_X_USER_VALUE;

    std::optional<boost::json::object> response;
    rpc::ClioError error = rpc::ClioError::etlCONNECTION_ERROR;
    while (numAttempts < sources_.size()) {
        auto res = sources_[sourceIdx]->forwardToRippled(request, clientIp, xUserValue, yield);
        if (res) {
            response = std::move(res).value();
            break;
        }
        error = std::max(error, res.error());  // Choose the best result between all sources

        sourceIdx = (sourceIdx + 1) % sources_.size();
        ++numAttempts;
    }

    if (response) {
        if (forwardingCache_ and not response->contains("error"))
            forwardingCache_->put(cmd, *response);
        return std::move(response).value();
    }

    return std::unexpected{error};
}

boost::json::value
LoadBalancer::toJson() const
{
    boost::json::array ret;
    for (auto& src : sources_)
        ret.push_back(src->toJson());

    return ret;
}

template <typename Func>
void
LoadBalancer::execute(Func f, uint32_t ledgerSequence, std::chrono::steady_clock::duration retryAfter)
{
    ASSERT(not sources_.empty(), "ETL sources must be configured to execute functions.");
    size_t sourceIdx = util::Random::uniform(0ul, sources_.size() - 1);

    size_t numAttempts = 0;

    while (true) {
        auto& source = sources_[sourceIdx];

        LOG(log_.debug()) << "Attempting to execute func. ledger sequence = " << ledgerSequence
                          << " - source = " << source->toString();
        // Originally, it was (source->hasLedger(ledgerSequence) || true)
        /* Sometimes rippled has ledger but doesn't actually know. However,
        but this does NOT happen in the normal case and is safe to remove
        This || true is only needed when loading full history standalone */
        if (source->hasLedger(ledgerSequence)) {
            bool const res = f(source);
            if (res) {
                LOG(log_.debug()) << "Successfully executed func at source = " << source->toString()
                                  << " - ledger sequence = " << ledgerSequence;
                break;
            }

            LOG(log_.warn()) << "Failed to execute func at source = " << source->toString()
                             << " - ledger sequence = " << ledgerSequence;
        } else {
            LOG(log_.warn()) << "Ledger not present at source = " << source->toString()
                             << " - ledger sequence = " << ledgerSequence;
        }
        sourceIdx = (sourceIdx + 1) % sources_.size();
        numAttempts++;
        if (numAttempts % sources_.size() == 0) {
            LOG(log_.info()) << "Ledger sequence " << ledgerSequence
                             << " is not yet available from any configured sources. Sleeping and trying again";
            std::this_thread::sleep_for(retryAfter);
        }
    }
}

std::optional<ETLState>
LoadBalancer::getETLState() noexcept
{
    if (!etlState_) {
        // retry ETLState fetch
        etlState_ = ETLState::fetchETLStateFromSource(*this);
    }
    return etlState_;
}

void
LoadBalancer::chooseForwardingSource()
{
    LOG(log_.info()) << "Choosing a new source to forward subscriptions";
    auto hasForwardingSourceLock = hasForwardingSource_.lock();
    hasForwardingSourceLock.get() = false;
    for (auto& source : sources_) {
        if (not hasForwardingSourceLock.get() and source->isConnected()) {
            source->setForwarding(true);
            hasForwardingSourceLock.get() = true;
        } else {
            source->setForwarding(false);
        }
    }
}

}  // namespace etl
