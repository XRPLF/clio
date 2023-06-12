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

#pragma once

#include <backend/BackendInterface.h>
#include <etl/Source.h>
#include <log/Logger.h>

#include <ripple/ledger/ReadView.h>
#include "org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h"
#include <grpcpp/grpcpp.h>

#include <optional>

namespace clio::detail {

/**
 * @brief GRPC Ledger data fetcher
 */
template <typename LoadBalancerType>
class LedgerFetcher
{
public:
    using OptionalGetLedgerResponseType = typename LoadBalancerType::OptionalGetLedgerResponseType;

private:
    clio::Logger log_{"ETL"};

    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<LoadBalancerType> loadBalancer_;

public:
    /**
     * @brief Create an instance of the fetcher
     */
    LedgerFetcher(std::shared_ptr<BackendInterface> backend, std::shared_ptr<LoadBalancerType> balancer)
        : backend_(backend), loadBalancer_(balancer)
    {
    }

    /**
     * @brief Extract data for a particular ledger from an ETL source
     *
     * This function continously tries to extract the specified ledger (using all available ETL sources) until the
     * extraction succeeds, or the server shuts down.
     *
     * @param sequence sequence of the ledger to extract
     * @return ledger header and transaction+metadata blobs; empty optional if the server is shutting down
     */
    OptionalGetLedgerResponseType
    fetchData(uint32_t seq)
    {
        log_.debug() << "Attempting to fetch ledger with sequence = " << seq;

        auto response = loadBalancer_->fetchLedger(seq, false, false);
        if (response)
            log_.trace() << "GetLedger reply = " << response->DebugString();
        return response;
    }

    /**
     * @brief Extract diff data for a particular ledger from an ETL source.
     *
     * This function continously tries to extract the specified ledger (using all available ETL sources) until the
     * extraction succeeds, or the server shuts down.
     *
     * @param sequence sequence of the ledger to extract
     * @return ledger header, transaction+metadata blobs, and all ledger objects created, modified or deleted between
     * this ledger and the parent; Empty optional if the server is shutting down
     */
    OptionalGetLedgerResponseType
    fetchDataAndDiff(uint32_t seq)
    {
        log_.debug() << "Attempting to fetch ledger with sequence = " << seq;

        auto response = loadBalancer_->fetchLedger(
            seq, true, !backend_->cache().isFull() || backend_->cache().latestLedgerSequence() >= seq);
        if (response)
            log_.trace() << "GetLedger reply = " << response->DebugString();

        return response;
    }
};

}  // namespace clio::detail
