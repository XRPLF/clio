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

#include "etl/InitialLoadObserverInterface.hpp"
#include "etl/LoadBalancerInterface.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <grpcpp/support/status.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace etl::impl {

class GrpcSource {
    util::Logger log_;
    std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub> stub_;
    std::unique_ptr<std::atomic_bool> initialLoadShouldStop_;
    std::chrono::system_clock::duration deadline_;

    static constexpr auto kKEEPALIVE_PING_INTERVAL_MS = 10000;
    static constexpr auto kKEEPALIVE_TIMEOUT_MS = 5000;
    static constexpr auto kKEEPALIVE_PERMIT_WITHOUT_CALLS = true;  // Allow keepalive pings when no calls
    static constexpr auto kMAX_PINGS_WITHOUT_DATA = 0;             // No limit
    static constexpr auto kDEADLINE = std::chrono::seconds(30);

public:
    GrpcSource(
        std::string const& ip,
        std::string const& grpcPort,
        std::chrono::system_clock::duration deadline = kDEADLINE
    );

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
    fetchLedger(uint32_t sequence, bool getObjects = true, bool getObjectNeighbors = false);

    /**
     * @brief Download a ledger in full.
     *
     * @param sequence Sequence of the ledger to download
     * @param numMarkers Number of markers to generate for async calls
     * @param observer InitialLoadObserverInterface implementation
     * @return Downloaded data or an indication of error or cancellation
     */
    InitialLedgerLoadResult
    loadInitialLedger(uint32_t sequence, uint32_t numMarkers, InitialLoadObserverInterface& observer);

    /**
     * @brief Stop any ongoing operations
     * @note This is used to cancel any ongoing initial ledger downloads
     * @param yield The coroutine context
     */
    void
    stop(boost::asio::yield_context yield);
};

}  // namespace etl::impl
