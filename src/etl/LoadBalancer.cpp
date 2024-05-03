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
#include "etl/ETLHelpers.hpp"
#include "etl/ETLState.hpp"
#include "etl/Source.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "util/Constants.hpp"
#include "util/Random.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace util;

namespace etl {

std::shared_ptr<LoadBalancer>
LoadBalancer::make_LoadBalancer(
    Config const& config,
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
    Config const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
    SourceFactory sourceFactory
)
{
    auto const forwardingCacheTimeout = config.valueOr<float>("forwarding_cache_timeout", 0.f);
    if (forwardingCacheTimeout > 0.f) {
        forwardingCache_ = impl::ForwardingCache{std::chrono::milliseconds{
            std::lroundf(forwardingCacheTimeout * static_cast<float>(util::MILLISECONDS_PER_SECOND))
        }};
    }

    static constexpr std::uint32_t MAX_DOWNLOAD = 256;
    if (auto value = config.maybeValue<uint32_t>("num_markers"); value) {
        downloadRanges_ = std::clamp(*value, 1u, MAX_DOWNLOAD);
    } else if (backend->fetchLedgerRange()) {
        downloadRanges_ = 4;
    }

    auto const allowNoEtl = config.valueOr("allow_no_etl", false);

    auto const checkOnETLFailure = [this, allowNoEtl](std::string const& log) {
        LOG(log_.warn()) << log;

        if (!allowNoEtl) {
            LOG(log_.error()) << "Set allow_no_etl as true in config to allow clio run without valid ETL sources.";
            throw std::logic_error("ETL configuration error.");
        }
    };

    for (auto const& entry : config.array("etl_sources")) {
        auto source = sourceFactory(
            entry,
            ioc,
            backend,
            subscriptions,
            validatedLedgers,
            [this]() {
                if (not hasForwardingSource_)
                    chooseForwardingSource();
            },
            [this]() { chooseForwardingSource(); },
            [this]() { forwardingCache_->invalidate(); }
        );

        // checking etl node validity
        auto const stateOpt = ETLState::fetchETLStateFromSource(*source);

        if (!stateOpt) {
            checkOnETLFailure(fmt::format(
                "Failed to fetch ETL state from source = {} Please check the configuration and network",
                source->toString()
            ));
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

std::pair<std::vector<std::string>, bool>
LoadBalancer::loadInitialLedger(uint32_t sequence, bool cacheOnly)
{
    std::vector<std::string> response;
    auto const success = execute(
        [this, &response, &sequence, cacheOnly](auto& source) {
            auto [data, res] = source->loadInitialLedger(sequence, downloadRanges_, cacheOnly);

            if (!res) {
                LOG(log_.error()) << "Failed to download initial ledger." << " Sequence = " << sequence
                                  << " source = " << source->toString();
            } else {
                response = std::move(data);
            }

            return res;
        },
        sequence
    );
    return {std::move(response), success};
}

LoadBalancer::OptionalGetLedgerResponseType
LoadBalancer::fetchLedger(uint32_t ledgerSequence, bool getObjects, bool getObjectNeighbors)
{
    GetLedgerResponseType response;
    bool const success = execute(
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
        ledgerSequence
    );
    if (success) {
        return response;
    }
    return {};
}

std::optional<boost::json::object>
LoadBalancer::forwardToRippled(
    boost::json::object const& request,
    std::optional<std::string> const& clientIp,
    boost::asio::yield_context yield
)
{
    if (forwardingCache_) {
        if (auto cachedResponse = forwardingCache_->get(request); cachedResponse) {
            return cachedResponse;
        }
    }

    std::size_t sourceIdx = 0;
    if (!sources_.empty())
        sourceIdx = util::Random::uniform(0ul, sources_.size() - 1);

    auto numAttempts = 0u;

    std::optional<boost::json::object> response;
    while (numAttempts < sources_.size()) {
        if (auto res = sources_[sourceIdx]->forwardToRippled(request, clientIp, yield)) {
            response = std::move(res);
            break;
        }

        sourceIdx = (sourceIdx + 1) % sources_.size();
        ++numAttempts;
    }

    if (response and forwardingCache_ and not response->contains("error"))
        forwardingCache_->put(request, *response);

    return response;
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
bool
LoadBalancer::execute(Func f, uint32_t ledgerSequence)
{
    std::size_t sourceIdx = 0;
    if (!sources_.empty())
        sourceIdx = util::Random::uniform(0ul, sources_.size() - 1);

    auto numAttempts = 0;

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
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    return true;
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
    hasForwardingSource_ = false;
    for (auto& source : sources_) {
        if (source->isConnected()) {
            source->setForwarding(true);
            hasForwardingSource_ = true;
            return;
        }
    }
}

}  // namespace etl
