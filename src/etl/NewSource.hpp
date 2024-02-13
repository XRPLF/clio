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

#include "data/BackendInterface.hpp"
#include "etl/ETLHelpers.hpp"
#include "etl/impl/ForwardingSource.hpp"
#include "etl/impl/GrpcSource.hpp"
#include "etl/impl/SubscriptionSource.hpp"
#include "feed/SubscriptionManager.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/uuid/uuid.hpp>
#include <grpcpp/support/status.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace etl {

template <
    typename GrpcSourceType = impl::GrpcSource,
    typename SubscriptionSourceTypePtr = std::unique_ptr<impl::SubscriptionSource>,
    typename ForwardingSourceType = impl::ForwardingSource>
class NewSourceImpl {
    std::string ip_;
    std::string wsPort_;
    std::string grpcPort_;

    GrpcSourceType grpcSource_;
    SubscriptionSourceTypePtr subscriptionSource_;
    ForwardingSourceType forwardingSource_;

public:
    using OnDisconnectHook = impl::SubscriptionSource::OnDisconnectHook;

    template <typename SomeGrpcSourceType, typename SomeForwardingSourceType>
        requires std::is_same_v<GrpcSourceType, SomeGrpcSourceType> &&
                     std::is_same_v<ForwardingSourceType, SomeForwardingSourceType>
    NewSourceImpl(
        std::string ip,
        std::string wsPort,
        std::string grpcPort,
        SomeGrpcSourceType&& grpcSource,
        SubscriptionSourceTypePtr subscriptionSource,
        SomeForwardingSourceType&& forwardingSource
    )
        : ip_(std::move(ip))
        , wsPort_(std::move(wsPort))
        , grpcPort_(std::move(grpcPort))
        , grpcSource_(std::forward<SomeGrpcSourceType>(grpcSource))
        , subscriptionSource_(std::move(subscriptionSource))
        , forwardingSource_(std::forward<SomeForwardingSourceType>(forwardingSource))
    {
    }

    /** @return true if source is connected; false otherwise */
    bool
    isConnected() const
    {
        return subscriptionSource_->isConnected();
    }

    /**
     * @brief Set the forwarding state of the source.
     *
     * @param isForwarding Whether to forward or not
     */
    void
    setForwarding(bool isForwarding)
    {
        subscriptionSource_->setForwarding(isForwarding);
    }

    /** @return JSON representation of the source */
    boost::json::object
    toJson() const
    {
        boost::json::object res;

        res["validated_range"] = subscriptionSource_->validatedRange();
        res["is_connected"] = std::to_string(static_cast<int>(subscriptionSource_->isConnected()));
        res["ip"] = ip_;
        res["ws_port"] = wsPort_;
        res["grpc_port"] = grpcPort_;

        auto last = subscriptionSource_->lastMessageTime();
        if (last.time_since_epoch().count() != 0) {
            res["last_msg_age_seconds"] = std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - last).count()
            );
        }

        return res;
    }

    /** @return String representation of the source (for debug) */
    std::string
    toString() const
    {
        return "{validated range: " + subscriptionSource_->validatedRange() + ", ip: " + ip_ +
            ", web socket port: " + wsPort_ + ", grpc port: " + grpcPort_ + "}";
    }

    /**
     * @brief Check if ledger is known by this source.
     *
     * @param sequence The ledger sequence to check
     * @return true if ledger is in the range of this source; false otherwise
     */
    bool
    hasLedger(uint32_t sequence) const
    {
        return subscriptionSource_->hasLedger(sequence);
    }

    /**
     * @brief Fetch data for a specific ledger.
     *
     * This function will continuously try to fetch data for the specified ledger until the fetch succeeds, the ledger
     * is found in the database, or the server is shutting down.
     *
     * @param sequence Sequence of the ledger to fetch
     * @param getObjects Whether to get the account state diff between this ledger and the prior one; defaults to true
     * @param getObjectNeighbors Whether to request object neighbors; defaults to false
     * @return A std::pair of the response status and the response itself
     */
    std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(uint32_t sequence, bool getObjects = true, bool getObjectNeighbors = false)
    {
        return grpcSource_.fetchLedger(sequence, getObjects, getObjectNeighbors);
    }

    /**
     * @brief Download a ledger in full.
     *
     * @param sequence Sequence of the ledger to download
     * @param numMarkers Number of markers to generate for async calls
     * @param cacheOnly Only insert into cache, not the DB; defaults to false
     * @return A std::pair of the data and a bool indicating whether the download was successful
     */
    std::pair<std::vector<std::string>, bool>
    loadInitialLedger(uint32_t sequence, std::uint32_t numMarkers, bool cacheOnly = false)
    {
        return grpcSource_.loadInitialLedger(sequence, numMarkers, cacheOnly);
    }

    /**
     * @brief Forward a request to rippled.
     *
     * @param request The request to forward
     * @param clientIp IP of the client forwarding this request if known
     * @param yield The coroutine context
     * @return Response wrapped in an optional on success; nullopt otherwise
     */
    std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& forwardToRippledClientIp,
        boost::asio::yield_context yield
    ) const
    {
        return forwardingSource_.forwardToRippled(request, forwardToRippledClientIp, yield);
    }
};

extern template class NewSourceImpl<>;

using NewSource = NewSourceImpl<>;

/**
 * @brief Create a source
 *
 * @param config The configuration to use
 * @param ioc The io_context to run on
 * @param backend BackendInterface implementation
 * @param subscriptions Subscription manager
 * @param validatedLedgers The network validated ledgers data structure
 */
NewSource
make_NewSource(
    util::Config const& config,
    boost::asio::io_context& ioc,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<feed::SubscriptionManager> subscriptions,
    std::shared_ptr<NetworkValidatedLedgers> validatedLedgers,
    NewSource::OnDisconnectHook onDisconnect
);

}  // namespace etl
