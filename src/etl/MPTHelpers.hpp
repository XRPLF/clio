/** @file */
#pragma once

#include "data/DBHelpers.hpp"

#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>

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
getMPTHolderFromTx(ripple::TxMeta const& txMeta, ripple::STTx const& sttx);

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
 * record per distinct issuance, each carrying the full set of affected accounts. Failed
 * transactions are indexed only when an issuance ID is attached. Used by live ETL and reused by the
 * historical backfill migrator.
 *
 * @param txMeta Transaction metadata
 * @param sttx The transaction
 * @return One record per distinct MPT issuance affected by the transaction; empty if the
 * transaction did not succeed or did not affect any MPT issuance
 */
std::vector<MPTokenIssuanceTransactionsData>
getMPTokenIssuanceTxsFromTx(ripple::TxMeta const& txMeta, ripple::STTx const& sttx);

}  // namespace etl
