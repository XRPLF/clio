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

#include <etl/Source.h>
#include <util/config/Config.h>
#include <util/log/Logger.h>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <mutex>

namespace etl {

/**
 * @brief This Source implementation attempts to connect over both secure websocket and plain websocket.
 *
 * First to connect pauses the other and the probing is considered done at this point.
 * If however the connected source loses connection the probing is kickstarted again.
 */
class ProbingSource : public Source
{
public:
    // TODO: inject when unit tests will be written for ProbingSource
    using GetLedgerResponseType = org::xrpl::rpc::v1::GetLedgerResponse;

private:
    util::Logger log_{"ETL"};

    std::mutex mtx_;
    boost::asio::ssl::context sslCtx_;
    std::shared_ptr<Source> sslSrc_;
    std::shared_ptr<Source> plainSrc_;
    std::shared_ptr<Source> currentSrc_;

public:
    /**
     * @brief Create an instance of the probing source
     *
     * @param config The configuration to use
     * @param ioc io context to run on
     * @param backend BackendInterface implementation
     * @param subscriptions Subscription manager
     * @param nwvl The network validated ledgers datastructure
     * @param balancer Load balancer to use
     * @param sslCtx The SSL context to use; defaults to tlsv12
     */
    ProbingSource(
        util::Config const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<feed::SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> nwvl,
        LoadBalancer& balancer,
        boost::asio::ssl::context sslCtx = boost::asio::ssl::context{boost::asio::ssl::context::tlsv12});

    ~ProbingSource() = default;

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

    std::pair<std::vector<std::string>, bool>
    loadInitialLedger(std::uint32_t ledgerSequence, std::uint32_t numMarkers, bool cacheOnly = false) override;

    std::pair<grpc::Status, GetLedgerResponseType>
    fetchLedger(uint32_t ledgerSequence, bool getObjects = true, bool getObjectNeighbors = false) override;

    std::optional<boost::json::object>
    forwardToRippled(boost::json::object const& request, std::string const& clientIp, boost::asio::yield_context yield)
        const override;

    boost::uuids::uuid
    token() const override;

private:
    std::optional<boost::json::object>
    requestFromRippled(
        boost::json::object const& request,
        std::string const& clientIp,
        boost::asio::yield_context yield) const override;

    SourceHooks
    make_SSLHooks() noexcept;

    SourceHooks
    make_PlainHooks() noexcept;
};
}  // namespace etl