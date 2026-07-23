#include "data/DBHelpers.hpp"
#include "util/Assert.hpp"

#include <boost/container/flat_set.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STIssue.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxMeta.h>

#include <optional>
#include <string>
#include <vector>

namespace etl {

std::vector<MPTHolderData>
getMPTHolderFromTx(xrpl::TxMeta const& txMeta, xrpl::STTx const&)
{
    if (txMeta.getResultTER() != xrpl::tesSUCCESS)
        return {};

    std::vector<MPTHolderData> holders;

    for (xrpl::STObject const& node : txMeta.getNodes()) {
        if (node.getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltMPTOKEN)
            continue;

        if (node.getFName() == xrpl::sfCreatedNode) {
            auto const& newMPT = node.peekAtField(xrpl::sfNewFields).downcast<xrpl::STObject>();
            holders.push_back(
                MPTHolderData{
                    .mptID = newMPT[xrpl::sfMPTokenIssuanceID],
                    .holder = newMPT.getAccountID(xrpl::sfAccount)
                }
            );
        }
    }

    return holders;
}

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

std::vector<MPTokenIssuanceTransactionsData>
getMPTokenIssuanceTxsFromTx(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx)
{
    // Collect each distinct issuance only once per transaction; the same set of affected accounts
    // is attached to every record produced below.
    MPTokenIssuanceIDs issuanceIDs;

    if (txMeta.getResultTER() == xrpl::tesSUCCESS) {
        for (auto const& node : txMeta.getNodes()) {
            if (auto const issuanceID = getMPTokenIssuanceIDFromNode(node); issuanceID.has_value())
                issuanceIDs.insert(*issuanceID);
        }
    }

    addMPTokenIssuanceIDsFromTx(issuanceIDs, sttx);

    if (issuanceIDs.empty())
        return {};

    auto const accounts = txMeta.getAffectedAccounts();

    std::vector<MPTokenIssuanceTransactionsData> result;
    result.reserve(issuanceIDs.size());
    for (auto const& issuanceID : issuanceIDs) {
        result.push_back(
            MPTokenIssuanceTransactionsData{
                .mptIssuanceID = issuanceID,
                .accounts = accounts,
                .ledgerSequence = txMeta.getLgrSeq(),
                .transactionIndex = txMeta.getIndex(),
                .txHash = sttx.getTransactionID()
            }
        );
    }
    return result;
}

std::optional<MPTHolderData>
getMPTHolderFromObj(std::string const& key, std::string const& blob)
{
    // https://github.com/XRPLF/XRPL-Standards/tree/master/XLS-0033-multi-purpose-tokens#2121-mptoken-ledger-identifier
    ASSERT(
        key.size() == xrpl::uint256::size(),
        "The size of the key is expected to fit uint256 exactly"
    );

    xrpl::STLedgerEntry const sle = xrpl::STLedgerEntry(
        xrpl::SerialIter{blob.data(), blob.size()}, xrpl::uint256::fromVoid(key.data())
    );

    if (sle.getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltMPTOKEN)
        return std::nullopt;

    auto const mptIssuanceID = sle[xrpl::sfMPTokenIssuanceID];
    auto const holder = sle.getAccountID(xrpl::sfAccount);

    return MPTHolderData{.mptID = mptIssuanceID, .holder = holder};
}

}  // namespace etl
