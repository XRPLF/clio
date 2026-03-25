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
     * This function continuously tries to extract the specified ledger (using all available ETL
     * sources) until the extraction succeeds, or the server shuts down.
     *
     * @param seq sequence of the ledger to extract
     * @return Ledger header and transaction+metadata blobs; Empty optional if the server is
     * shutting down
     */
    [[nodiscard]] virtual OptionalGetLedgerResponseType
    fetchData(uint32_t seq) = 0;

    /**
     * @brief Extract diff data for a particular ledger from an ETL source.
     *
     * This function continuously tries to extract the specified ledger (using all available ETL
     * sources) until the extraction succeeds, or the server shuts down.
     *
     * @param seq sequence of the ledger to extract
     * @return Ledger data diff between sequance and parent; Empty optional if the server is
     * shutting down
     */
    [[nodiscard]] virtual OptionalGetLedgerResponseType
    fetchDataAndDiff(uint32_t seq) = 0;
};

}  // namespace etl
