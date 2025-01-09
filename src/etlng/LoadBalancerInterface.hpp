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

/** @file */
#pragma once

#include "etl/ETLState.hpp"
#include "etlng/InitialLoadObserverInterface.hpp"
#include "rpc/Errors.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace etlng {

/**
 * @brief An interface for LoadBalancer
 */
class LoadBalancerInterface {
public:
    using RawLedgerObjectType = org::xrpl::rpc::v1::RawLedgerObject;
    using GetLedgerResponseType = org::xrpl::rpc::v1::GetLedgerResponse;
    using OptionalGetLedgerResponseType = std::optional<GetLedgerResponseType>;

    virtual ~LoadBalancerInterface() = default;

    /**
     * @brief Load the initial ledger, writing data to the queue.
     * @note This function will retry indefinitely until the ledger is downloaded.
     *
     * @param sequence Sequence of ledger to download
     * @param loader InitialLoadObserverInterface implementation
     * @param retryAfter Time to wait between retries (2 seconds by default)
     * @return A std::vector<std::string> The ledger data
     */
    virtual std::vector<std::string>
    loadInitialLedger(
        uint32_t sequence,
        etlng::InitialLoadObserverInterface& loader,
        std::chrono::steady_clock::duration retryAfter = std::chrono::seconds{2}
    ) = 0;

    /**
     * @brief Load the initial ledger, writing data to the queue.
     * @note This function will retry indefinitely until the ledger is downloaded.
     *
     * @param sequence Sequence of ledger to download
     * @param retryAfter Time to wait between retries (2 seconds by default)
     * @return A std::vector<std::string> The ledger data
     */
    virtual std::vector<std::string>
    loadInitialLedger(uint32_t sequence, std::chrono::steady_clock::duration retryAfter = std::chrono::seconds{2}) = 0;

    /**
     * @brief Fetch data for a specific ledger.
     *
     * This function will continuously try to fetch data for the specified ledger until the fetch succeeds, the ledger
     * is found in the database, or the server is shutting down.
     *
     * @param ledgerSequence Sequence of the ledger to fetch
     * @param getObjects Whether to get the account state diff between this ledger and the prior one
     * @param getObjectNeighbors Whether to request object neighbors
     * @param retryAfter Time to wait between retries (2 seconds by default)
     * @return The extracted data, if extraction was successful. If the ledger was found
     * in the database or the server is shutting down, the optional will be empty
     */
    virtual OptionalGetLedgerResponseType
    fetchLedger(
        uint32_t ledgerSequence,
        bool getObjects,
        bool getObjectNeighbors,
        std::chrono::steady_clock::duration retryAfter = std::chrono::seconds{2}
    ) = 0;

    /**
     * @brief Represent the state of this load balancer as a JSON object
     *
     * @return JSON representation of the state of this load balancer.
     */
    virtual boost::json::value
    toJson() const = 0;

    /**
     * @brief Forward a JSON RPC request to a randomly selected rippled node.
     *
     * @param request JSON-RPC request to forward
     * @param clientIp The IP address of the peer, if known
     * @param isAdmin Whether the request is from an admin
     * @param yield The coroutine context
     * @return Response received from rippled node as JSON object on success or error on failure
     */
    virtual std::expected<boost::json::object, rpc::ClioError>
    forwardToRippled(
        boost::json::object const& request,
        std::optional<std::string> const& clientIp,
        bool isAdmin,
        boost::asio::yield_context yield
    ) = 0;

    /**
     * @brief Return state of ETL nodes.
     * @return ETL state, nullopt if etl nodes not available
     */
    virtual std::optional<etl::ETLState>
    getETLState() noexcept = 0;
};

}  // namespace etlng
