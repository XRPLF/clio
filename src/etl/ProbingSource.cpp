//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "etl/ProbingSource.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/json/object.hpp>
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include "data/BackendInterface.h"
#include "etl/ETLHelpers.h"
#include "etl/LoadBalancer.h"
#include "etl/Source.h"
#include "feed/SubscriptionManager.h"
#include "util/config/Config.h"
#include "util/log/Logger.h"
#include <cstdint>
#include <functional>
#include <grpcpp/support/status.h>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace etl {

ProbingSource::ProbingSource(
    util::Config const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<feed::SubscriptionManager> subscriptions,
    std::shared_ptr<NetworkValidatedLedgers> nwvl,
    LoadBalancer& balancer,
    boost::asio::ssl::context sslCtx
)
    : sslCtx_{std::move(sslCtx)}
    , sslSrc_{make_shared<
          SslSource>(config, ioc, std::ref(sslCtx_), backend, subscriptions, nwvl, balancer, make_SSLHooks())}
    , plainSrc_{make_shared<PlainSource>(config, ioc, backend, subscriptions, nwvl, balancer, make_PlainHooks())}
{
}

void
ProbingSource::run()
{
    sslSrc_->run();
    plainSrc_->run();
}

void
ProbingSource::pause()
{
    sslSrc_->pause();
    plainSrc_->pause();
}

void
ProbingSource::resume()
{
    sslSrc_->resume();
    plainSrc_->resume();
}

bool
ProbingSource::isConnected() const
{
    return currentSrc_ && currentSrc_->isConnected();
}

bool
ProbingSource::hasLedger(uint32_t sequence) const
{
    if (!currentSrc_)
        return false;
    return currentSrc_->hasLedger(sequence);
}

boost::json::object
ProbingSource::toJson() const
{
    if (!currentSrc_) {
        boost::json::object sourcesJson = {
            {"ws", plainSrc_->toJson()},
            {"wss", sslSrc_->toJson()},
        };

        return {
            {"probing", sourcesJson},
        };
    }
    return currentSrc_->toJson();
}

std::string
ProbingSource::toString() const
{
    if (!currentSrc_)
        return "{probing... ws: " + plainSrc_->toString() + ", wss: " + sslSrc_->toString() + "}";
    return currentSrc_->toString();
}

boost::uuids::uuid
ProbingSource::token() const
{
    if (!currentSrc_)
        return boost::uuids::nil_uuid();
    return currentSrc_->token();
}

std::pair<std::vector<std::string>, bool>
ProbingSource::loadInitialLedger(std::uint32_t sequence, std::uint32_t numMarkers, bool cacheOnly)
{
    if (!currentSrc_)
        return {{}, false};
    return currentSrc_->loadInitialLedger(sequence, numMarkers, cacheOnly);
}

std::pair<grpc::Status, ProbingSource::GetLedgerResponseType>
ProbingSource::fetchLedger(uint32_t sequence, bool getObjects, bool getObjectNeighbors)
{
    if (!currentSrc_)
        return {};
    return currentSrc_->fetchLedger(sequence, getObjects, getObjectNeighbors);
}

std::optional<boost::json::object>
ProbingSource::forwardToRippled(
    boost::json::object const& request,
    std::optional<std::string> const& clientIp,
    boost::asio::yield_context yield
) const
{
    if (!currentSrc_)  // Source may connect to rippled before the connection built to check the validity
    {
        if (auto res = plainSrc_->forwardToRippled(request, clientIp, yield))
            return res;

        return sslSrc_->forwardToRippled(request, clientIp, yield);
    }
    return currentSrc_->forwardToRippled(request, clientIp, yield);
}

std::optional<boost::json::object>
ProbingSource::requestFromRippled(
    boost::json::object const& request,
    std::optional<std::string> const& clientIp,
    boost::asio::yield_context yield
) const
{
    if (!currentSrc_)
        return {};
    return currentSrc_->requestFromRippled(request, clientIp, yield);
}

SourceHooks
ProbingSource::make_SSLHooks() noexcept
{
    return {// onConnected
            [this](auto ec) {
                std::lock_guard const lck(mtx_);
                if (currentSrc_)
                    return SourceHooks::Action::STOP;

                if (!ec) {
                    plainSrc_->pause();
                    currentSrc_ = sslSrc_;
                    LOG(log_.info()) << "Selected WSS as the main source: " << currentSrc_->toString();
                }
                return SourceHooks::Action::PROCEED;
            },
            // onDisconnected
            [this](auto /* ec */) {
                std::lock_guard const lck(mtx_);
                if (currentSrc_) {
                    currentSrc_ = nullptr;
                    plainSrc_->resume();
                }
                return SourceHooks::Action::STOP;
            }
    };
}

SourceHooks
ProbingSource::make_PlainHooks() noexcept
{
    return {// onConnected
            [this](auto ec) {
                std::lock_guard const lck(mtx_);
                if (currentSrc_)
                    return SourceHooks::Action::STOP;

                if (!ec) {
                    sslSrc_->pause();
                    currentSrc_ = plainSrc_;
                    LOG(log_.info()) << "Selected Plain WS as the main source: " << currentSrc_->toString();
                }
                return SourceHooks::Action::PROCEED;
            },
            // onDisconnected
            [this](auto /* ec */) {
                std::lock_guard const lck(mtx_);
                if (currentSrc_) {
                    currentSrc_ = nullptr;
                    sslSrc_->resume();
                }
                return SourceHooks::Action::STOP;
            }
    };
};
}  // namespace etl
