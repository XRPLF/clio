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

#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/ledger.pb.h>

#include <cstdint>
#include <optional>

namespace etl {

/**
 * @brief An interface for LedgerFetcher
 */
struct LedgerFetcherInterface {
    using GetLedgerResponseType = org::xrpl::rpc::v1::GetLedgerResponse;
    using OptionalGetLedgerResponseType = std::optional<GetLedgerResponseType>;

    virtual ~LedgerFetcherInterface() = default;

    /**
     * @brief Extract data for a particular ledger from an ETL source
     *
     * This function continously tries to extract the specified ledger (using all available ETL sources) until the
     * extraction succeeds, or the server shuts down.
     *
     * @param seq sequence of the ledger to extract
     * @return Ledger header and transaction+metadata blobs; Empty optional if the server is shutting down
     */
    [[nodiscard]] virtual OptionalGetLedgerResponseType
    fetchData(uint32_t seq) = 0;

    /**
     * @brief Extract diff data for a particular ledger from an ETL source.
     *
     * This function continously tries to extract the specified ledger (using all available ETL sources) until the
     * extraction succeeds, or the server shuts down.
     *
     * @param seq sequence of the ledger to extract
     * @return Ledger data diff between sequance and parent; Empty optional if the server is shutting down
     */
    [[nodiscard]] virtual OptionalGetLedgerResponseType
    fetchDataAndDiff(uint32_t seq) = 0;
};

}  // namespace etl
