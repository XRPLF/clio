#pragma once

#include <boost/container/flat_set.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxMeta.h>

#include <optional>

namespace util {

/**
 * @brief A set of distinct MPTokenIssuanceIDs.
 */
using MPTokenIssuanceIDs = boost::container::flat_set<xrpl::uint192>;

/**
 * @brief Derive the MPTokenIssuanceID from an affected node in transaction metadata.
 *
 * @param node An entry of the metadata's AffectedNodes array.
 * @return The 192-bit issuance ID if the node is an MPTokenIssuance or MPToken object.
 */
std::optional<xrpl::uint192>
getMPTokenIssuanceIDFromNode(xrpl::STObject const& node);

/**
 * @brief Collect every MPTokenIssuanceID referenced by a transaction's own fields.
 *
 * @note Checks the top-level sfMPTokenIssuanceID field, plus any STI_AMOUNT/STI_ISSUE field holding
 * an xrpl::MPTIssue (e.g. Payment's sfAmount, AMM's sfAsset/sfAsset2).
 *
 * @param [out] issuanceIDs Set to insert each found issuance ID into.
 * @param sttx The transaction.
 */
void
addMPTokenIssuanceIDsFromTx(MPTokenIssuanceIDs& issuanceIDs, xrpl::STTx const& sttx);

/**
 * @brief Check whether a transaction references a specific MPT issuance.
 *
 * @note Scans the transaction's metadata for affected MPTokenIssuance/MPToken nodes, and the
 * transaction's own MPTokenIssuanceID/MPT issue fields, exiting as soon as a match is found.
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

}  // namespace util
