/** @file */
#pragma once

#include "data/DBHelpers.hpp"

#include <boost/container/flat_set.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STIssue.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TER.h>
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

namespace {

using MPTokenIssuanceIDs = boost::container::flat_set<xrpl::uint192>;

/**
 * @brief Derive the MPTokenIssuanceID from an affected node in transaction metadata.
 *
 * @param node An entry of the metadata's AffectedNodes array.
 * @return The 192-bit issuance ID if the node is an MPTokenIssuance or MPToken object.
 */
std::optional<xrpl::uint192>
getMPTokenIssuanceIDFromNode(xrpl::STObject const& node)
{
    auto const entryType = node.getFieldU16(xrpl::sfLedgerEntryType);
    if (entryType != xrpl::ltMPTOKEN && entryType != xrpl::ltMPTOKEN_ISSUANCE)
        return std::nullopt;

    auto const& fieldsName =
        node.getFName() == xrpl::sfCreatedNode ? xrpl::sfNewFields : xrpl::sfFinalFields;
    if (not node.isFieldPresent(fieldsName))
        return std::nullopt;

    auto const& fields = node.peekAtField(fieldsName).downcast<xrpl::STObject>();

    if (entryType == xrpl::ltMPTOKEN) {
        if (not fields.isFieldPresent(xrpl::sfMPTokenIssuanceID))
            return std::nullopt;

        return fields[xrpl::sfMPTokenIssuanceID];
    }

    // MPTokenIssuance objects carry no sfMPTokenIssuanceID, and the node's ledger key is a
    // one-way hash that does not embed the ID, so reconstruct it from sfSequence and sfIssuer
    if (not fields.isFieldPresent(xrpl::sfSequence) || not fields.isFieldPresent(xrpl::sfIssuer))
        return std::nullopt;

    return xrpl::makeMptID(
        fields.getFieldU32(xrpl::sfSequence), fields.getAccountID(xrpl::sfIssuer)
    );
}

void
addMPTokenIssuanceIDsFromTx(MPTokenIssuanceIDs& issuanceIDs, xrpl::STTx const& sttx)
{
    if (sttx.isFieldPresent(xrpl::sfMPTokenIssuanceID))
        issuanceIDs.insert(sttx.getFieldH192(xrpl::sfMPTokenIssuanceID));

    for (xrpl::STBase const& field : sttx) {
        switch (field.getSType()) {
            case xrpl::STI_AMOUNT: {
                auto const& amount = field.downcast<xrpl::STAmount>();
                if (amount.holds<xrpl::MPTIssue>())
                    issuanceIDs.insert(amount.get<xrpl::MPTIssue>().getMptID());
                break;
            }
            case xrpl::STI_ISSUE: {
                auto const& issue = field.downcast<xrpl::STIssue>();
                if (issue.holds<xrpl::MPTIssue>())
                    issuanceIDs.insert(issue.value().get<xrpl::MPTIssue>().getMptID());
                break;
            }
            default:
                break;
        }
    }
}

}  // namespace

/**
 * @brief Check whether a transaction references a specific MPT issuance.
 *
 * @note Scans the same sources as getMPTokenIssuanceTxsFromTx (metadata's affected
 * MPTokenIssuance/MPToken nodes, and the transaction's own MPTokenIssuanceID/MPT issue fields), but
 * exits as soon as a match is found instead of collecting every distinct issuance touched. Defined
 * inline (header-only) so callers do not need to link the clio_etl library for this predicate.
 *
 * @param txMeta Transaction metadata.
 * @param sttx The transaction.
 * @param mptIssuanceID The MPT issuance to check for.
 * @return true if the transaction references mptIssuanceID.
 */
inline bool
referencesMptIssuance(
    xrpl::TxMeta const& txMeta,
    xrpl::STTx const& sttx,
    xrpl::uint192 const& mptIssuanceID
)
{
    if (txMeta.getResultTER() == xrpl::tesSUCCESS) {
        for (auto const& node : txMeta.getNodes()) {
            if (getMPTokenIssuanceIDFromNode(node) == mptIssuanceID)
                return true;
        }
    }

    MPTokenIssuanceIDs issuanceIDs;
    addMPTokenIssuanceIDsFromTx(issuanceIDs, sttx);
    return issuanceIDs.contains(mptIssuanceID);
}

}  // namespace etl
