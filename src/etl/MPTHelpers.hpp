/** @file */
#pragma once

#include "data/DBHelpers.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxMeta.h>

#include <optional>
#include <string>
#include <vector>

namespace etl {

/**
 * @brief Pull MPT data from TX via ETLService.
 *
 * @param txMeta Transaction metadata
 * @param sttx The transaction
 * @return The MPTIssuanceID and holder pairs created by the transaction; empty if the transaction
 * failed or created no MPToken.
 */
std::vector<MPTHolderData>
getMPTHolderFromTx(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx);

/**
 * @brief Pull MPT data from ledger object via loadInitialLedger.
 *
 * @param key The owner key
 * @param blob Object data as blob
 * @return The MPTIssuanceID and holder pair as a optional
 */
std::optional<MPTHolderData>
getMPTHolderFromObj(std::string const& key, std::string const& blob);

/**
 * @brief Pull MPT issuance transaction index data from a transaction.
 *
 * @note This scans the transaction's metadata for affected MPTokenIssuance/MPToken ledger objects
 * and transaction fields for attached MPTokenIssuanceID/MPT issue references. It produces one
 * record per distinct issuance, each carrying the full set of affected accounts. Transaction fields
 * are scanned so failed transactions that carry an issuance reference are indexed even when
 * metadata has no affected MPT objects. Used by live ETL and reused by the historical backfill
 * migrator.
 *
 * @param txMeta Transaction metadata.
 * @param sttx The transaction.
 * @return One record per distinct MPT issuance referenced by metadata or transaction fields; empty
 * if no MPT issuance reference is found.
 */
std::vector<MPTokenIssuanceTransactionsData>
getMPTokenIssuanceTxsFromTx(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx);

/**
 * @brief Check whether a transaction references a specific MPT issuance.
 *
 * @note Scans the same sources as getMPTokenIssuanceTxsFromTx (metadata's affected
 * MPTokenIssuance/MPToken nodes, and the transaction's own MPTokenIssuanceID/MPT issue fields), but
 * exits as soon as a match is found instead of collecting every distinct issuance touched.
 *
 * @param txMeta Transaction metadata.
 * @param sttx The transaction.
 * @param mptIssuanceID The MPT issuance to check for.
 * @return true if the transaction references mptIssuanceID.
 */
bool
referencesMptIssuance(
    xrpl::TxMeta const& txMeta,
    xrpl::STTx const& sttx,
    xrpl::uint192 const& mptIssuanceID
);

}  // namespace etl
