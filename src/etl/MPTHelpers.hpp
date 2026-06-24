/** @file */
#pragma once

#include "data/DBHelpers.hpp"

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

}  // namespace etl
