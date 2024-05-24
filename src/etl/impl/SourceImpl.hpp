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

#include "etl/Source.hpp"
#include "etl/impl/ForwardingSource.hpp"
#include "etl/impl/GrpcSource.hpp"
#include "etl/impl/SubscriptionSource.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <grpcpp/support/status.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
namespace etl::impl {

/**
 * @brief Provides an implementation of a ETL source
 *
 * @tparam GrpcSourceType The type of the gRPC source
 * @tparam SubscriptionSourceTypePtr The type of the subscription source
 * @tparam ForwardingSourceType The type of the forwarding source
 */
template <
    typename GrpcSourceType = GrpcSource,
    typename SubscriptionSourceTypePtr = std::unique_ptr<SubscriptionSource>,
    typename ForwardingSourceType = ForwardingSource>
class SourceImpl : public SourceBase {
    std::string ip_;
    std::string wsPort_;
    std::string grpcPort_;

    GrpcSourceType grpcSource_;
    SubscriptionSourceTypePtr subscriptionSource_;
    ForwardingSourceType forwardingSource_;

public:
    /**
     * @brief Construct a new SourceImpl object
     *
     * @param ip The IP of the source
     * @param wsPort The web socket port of the source
     * @param grpcPort The gRPC port of the source
     * @param grpcSource The gRPC source
     * @param subscriptionSource The subscription source
     * @param forwardingSource The forwarding source
     */
    template <typename SomeGrpcSourceType, typename SomeForwardingSourceType>
        requires std::is_same_v<GrpcSourceType, SomeGrpcSourceType> and
                     std::is_same_v<ForwardingSourceType, SomeForwardingSourceType>
    SourceImpl(
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

    /**
     * @brief Run subscriptions loop of the source
     */
    void
    run() final
    {
        subscriptionSource_->run();
    }

    /**
     * @brief Check if source is connected
     *
     * @return true if source is connected; false otherwise
     */
    bool
    isConnected() const final
    {
        return subscriptionSource_->isConnected();
    }

    /**
     * @brief Set the forwarding state of the source.
     *
     * @param isForwarding Whether to forward or not
     */
    void
    setForwarding(bool isForwarding) final
    {
        subscriptionSource_->setForwarding(isForwarding);
    }

    /**
     * @brief Represent the source as a JSON object
     *
     * @return JSON representation of the source
     */
    boost::json::object
    toJson() const final
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
    toString() const final
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
    hasLedger(uint32_t sequence) const final
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
    fetchLedger(uint32_t sequence, bool getObjects = true, bool getObjectNeighbors = false) final
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
    loadInitialLedger(uint32_t sequence, std::uint32_t numMarkers, bool cacheOnly = false) final
    {
        return grpcSource_.loadInitialLedger(sequence, numMarkers, cacheOnly);
    }

    /**
     * @brief Forward a request to rippled.
     *
     * @param request The request to forward
     * @param forwardToRippledClientIp IP of the client forwarding this request if known
     * @param xUserValue Optional value of the X-User header
     * @param yield The coroutine context
     * @return Response wrapped in an optional on success; nullopt otherwise
     */
    std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& forwardToRippledClientIp,
        std::optional<std::string> const& xUserValue,
        boost::asio::yield_context yield
    ) const final
    {
        return forwardingSource_.forwardToRippled(request, forwardToRippledClientIp, xUserValue, yield);
    }
};

}  // namespace etl::impl
