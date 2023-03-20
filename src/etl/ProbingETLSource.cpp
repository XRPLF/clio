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

#include <etl/ProbingETLSource.h>
#include <log/Logger.h>

using namespace clio;

ProbingETLSource::ProbingETLSource(
    clio::Config const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<NetworkValidatedLedgers> nwvl,
    ETLLoadBalancer& balancer,
    boost::asio::ssl::context sslCtx)
    : sslCtx_{std::move(sslCtx)}
    , sslSrc_{make_shared<SslETLSource>(
          config,
          ioc,
          std::ref(sslCtx_),
          backend,
          subscriptions,
          nwvl,
          balancer,
          make_SSLHooks())}
    , plainSrc_{make_shared<PlainETLSource>(
          config,
          ioc,
          backend,
          subscriptions,
          nwvl,
          balancer,
          make_PlainHooks())}
{
}

void
ProbingETLSource::run()
{
    sslSrc_->run();
    plainSrc_->run();
}

void
ProbingETLSource::pause()
{
    sslSrc_->pause();
    plainSrc_->pause();
}

void
ProbingETLSource::resume()
{
    sslSrc_->resume();
    plainSrc_->resume();
}

bool
ProbingETLSource::isConnected() const
{
    return currentSrc_ && currentSrc_->isConnected();
}

bool
ProbingETLSource::hasLedger(uint32_t sequence) const
{
    if (!currentSrc_)
        return false;
    return currentSrc_->hasLedger(sequence);
}

boost::json::object
ProbingETLSource::toJson() const
{
    if (!currentSrc_)
    {
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
ProbingETLSource::toString() const
{
    if (!currentSrc_)
        return "{probing... ws: " + plainSrc_->toString() +
            ", wss: " + sslSrc_->toString() + "}";
    return currentSrc_->toString();
}

std::string
ProbingETLSource::token() const
{
    if (!currentSrc_)
        return "";
    return currentSrc_->token();
}

bool
ProbingETLSource::loadInitialLedger(
    std::uint32_t ledgerSequence,
    std::uint32_t numMarkers,
    bool cacheOnly)
{
    if (!currentSrc_)
        return false;
    return currentSrc_->loadInitialLedger(
        ledgerSequence, numMarkers, cacheOnly);
}

std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
ProbingETLSource::fetchLedger(
    uint32_t ledgerSequence,
    bool getObjects,
    bool getObjectNeighbors)
{
    if (!currentSrc_)
        return {};
    return currentSrc_->fetchLedger(
        ledgerSequence, getObjects, getObjectNeighbors);
}

std::optional<boost::json::object>
ProbingETLSource::forwardToRippled(
    boost::json::object const& request,
    std::string const& clientIp,
    boost::asio::yield_context& yield) const
{
    if (!currentSrc_)
        return {};
    return currentSrc_->forwardToRippled(request, clientIp, yield);
}

std::optional<boost::json::object>
ProbingETLSource::requestFromRippled(
    boost::json::object const& request,
    std::string const& clientIp,
    boost::asio::yield_context& yield) const
{
    if (!currentSrc_)
        return {};
    return currentSrc_->requestFromRippled(request, clientIp, yield);
}

ETLSourceHooks
ProbingETLSource::make_SSLHooks() noexcept
{
    return {// onConnected
            [this](auto ec) {
                std::lock_guard lck(mtx_);
                if (currentSrc_)
                    return ETLSourceHooks::Action::STOP;

                if (!ec)
                {
                    plainSrc_->pause();
                    currentSrc_ = sslSrc_;
                    log_.info() << "Selected WSS as the main source: "
                                << currentSrc_->toString();
                }
                return ETLSourceHooks::Action::PROCEED;
            },
            // onDisconnected
            [this](auto ec) {
                std::lock_guard lck(mtx_);
                if (currentSrc_)
                {
                    currentSrc_ = nullptr;
                    plainSrc_->resume();
                }
                return ETLSourceHooks::Action::STOP;
            }};
}

ETLSourceHooks
ProbingETLSource::make_PlainHooks() noexcept
{
    return {// onConnected
            [this](auto ec) {
                std::lock_guard lck(mtx_);
                if (currentSrc_)
                    return ETLSourceHooks::Action::STOP;

                if (!ec)
                {
                    sslSrc_->pause();
                    currentSrc_ = plainSrc_;
                    log_.info() << "Selected Plain WS as the main source: "
                                << currentSrc_->toString();
                }
                return ETLSourceHooks::Action::PROCEED;
            },
            // onDisconnected
            [this](auto ec) {
                std::lock_guard lck(mtx_);
                if (currentSrc_)
                {
                    currentSrc_ = nullptr;
                    sslSrc_->resume();
                }
                return ETLSourceHooks::Action::STOP;
            }};
}
