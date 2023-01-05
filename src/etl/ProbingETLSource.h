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

#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <mutex>

#include <etl/ETLSource.h>
#include <util/config/Config.h>
#include <util/log/Logger.h>

namespace clio::etl {

/// This ETLSource implementation attempts to connect over both secure websocket
/// and plain websocket. First to connect pauses the other and the probing is
/// considered done at this point. If however the connected source loses
/// connection the probing is kickstarted again.
class ProbingETLSource : public ETLSource
{
    util::Logger log_{"ETL"};

    std::mutex mtx_;
    boost::asio::ssl::context sslCtx_;
    std::shared_ptr<ETLSource> sslSrc_;
    std::shared_ptr<ETLSource> plainSrc_;
    std::shared_ptr<ETLSource> currentSrc_;

public:
    ProbingETLSource(
        util::Config const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<data::BackendInterface> backend,
        std::shared_ptr<feed::SubscriptionManager> subscriptions,
        std::shared_ptr<etl::NetworkValidatedLedgers> nwvl,
        ETLLoadBalancer& balancer,
        boost::asio::ssl::context sslCtx = boost::asio::ssl::context{
            boost::asio::ssl::context::tlsv12});

    ~ProbingETLSource() = default;

    void
    run() override;

    void
    pause() override;

    void
    resume() override;

    bool
    isConnected() const override;

    bool
    hasLedger(uint32_t sequence) const override;

    boost::json::object
    toJson() const override;

    std::string
    toString() const override;

    bool
    loadInitialLedger(
        std::uint32_t ledgerSequence,
        std::uint32_t numMarkers,
        bool cacheOnly = false) override;

    std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(
        uint32_t ledgerSequence,
        bool getObjects = true,
        bool getObjectNeighbors = false) override;

    std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::string const& clientIp,
        boost::asio::yield_context& yield) const override;

private:
    std::optional<boost::json::object>
    requestFromRippled(
        boost::json::object const& request,
        std::string const& clientIp,
        boost::asio::yield_context& yield) const override;

    ETLSourceHooks
    make_SSLHooks() noexcept;

    ETLSourceHooks
    make_PlainHooks() noexcept;
};

}  // namespace clio::etl
