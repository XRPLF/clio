#include "data/DBHelpers.hpp"
#include "util/Assert.hpp"

#include <boost/container/flat_set.hpp>
#include <ripple/protocol/STBase.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STIssue.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>

#include <optional>
#include <string>
#include <vector>

namespace etl {

std::vector<MPTHolderData>
getMPTHolderFromTx(ripple::TxMeta const& txMeta, ripple::STTx const&)
{
    if (txMeta.getResultTER() != ripple::tesSUCCESS)
        return {};

    std::vector<MPTHolderData> holders;

    for (ripple::STObject const& node : txMeta.getNodes()) {
        if (node.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltMPTOKEN)
            continue;

        if (node.getFName() == ripple::sfCreatedNode) {
            auto const& newMPT = node.peekAtField(ripple::sfNewFields).downcast<ripple::STObject>();
            holders.push_back(
                MPTHolderData{
                    .mptID = newMPT[ripple::sfMPTokenIssuanceID],
                    .holder = newMPT.getAccountID(ripple::sfAccount)
                }
            );
        }
    }

    return holders;
}

namespace {

using MPTokenIssuanceIDs = boost::container::flat_set<ripple::uint192>;

/**
 * @brief Derive the MPTokenIssuanceID from an affected node in transaction metadata
 *
 * @param node An entry of the metadata's AffectedNodes array
 * @return The 192-bit issuance ID if the node is an MPTokenIssuance or MPToken object
 */
std::optional<ripple::uint192>
getMPTokenIssuanceIDFromNode(ripple::STObject const& node)
{
    auto const entryType = node.getFieldU16(ripple::sfLedgerEntryType);
    if (entryType != ripple::ltMPTOKEN && entryType != ripple::ltMPTOKEN_ISSUANCE)
        return {};

    auto const& fieldsName =
        node.getFName() == ripple::sfCreatedNode ? ripple::sfNewFields : ripple::sfFinalFields;
    if (not node.isFieldPresent(fieldsName))
        return {};

    auto const& fields = node.peekAtField(fieldsName).downcast<ripple::STObject>();

    if (entryType == ripple::ltMPTOKEN) {
        if (not fields.isFieldPresent(ripple::sfMPTokenIssuanceID))
            return {};

        return fields[ripple::sfMPTokenIssuanceID];
    }

    // MPTokenIssuance objects carry no sfMPTokenIssuanceID, and the node's ledger key is a
    // one-way hash that does not embed the ID, so reconstruct it from sfSequence and sfIssuer
    if (not fields.isFieldPresent(ripple::sfSequence) ||
        not fields.isFieldPresent(ripple::sfIssuer))
        return {};

    return ripple::makeMptID(
        fields.getFieldU32(ripple::sfSequence), fields.getAccountID(ripple::sfIssuer)
    );
}

void
addMPTokenIssuanceIDFromAmount(MPTokenIssuanceIDs& issuanceIDs, ripple::STAmount const& amount)
{
    if (amount.holds<ripple::MPTIssue>())
        issuanceIDs.insert(amount.get<ripple::MPTIssue>().getMptID());
}

void
addMPTokenIssuanceIDFromIssue(MPTokenIssuanceIDs& issuanceIDs, ripple::STIssue const& issue)
{
    if (issue.holds<ripple::MPTIssue>())
        issuanceIDs.insert(issue.value().get<ripple::MPTIssue>().getMptID());
}

void
addMPTokenIssuanceIDsFromTx(MPTokenIssuanceIDs& issuanceIDs, ripple::STTx const& sttx)
{
    if (sttx.isFieldPresent(ripple::sfMPTokenIssuanceID))
        issuanceIDs.insert(sttx.getFieldH192(ripple::sfMPTokenIssuanceID));

    for (ripple::STBase const& field : sttx) {
        switch (field.getSType()) {
            case ripple::STI_AMOUNT:
                addMPTokenIssuanceIDFromAmount(issuanceIDs, field.downcast<ripple::STAmount>());
                break;
            case ripple::STI_ISSUE:
                addMPTokenIssuanceIDFromIssue(issuanceIDs, field.downcast<ripple::STIssue>());
                break;
            default:
                break;
        }
    }
}

}  // namespace

std::vector<MPTokenIssuanceTransactionsData>
getMPTokenIssuanceTxsFromTx(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    // Collect each distinct issuance only once per transaction; the same set of affected accounts
    // is attached to every record produced below.
    MPTokenIssuanceIDs issuanceIDs;
    for (ripple::STObject const& node : txMeta.getNodes()) {
        if (auto const issuanceID = getMPTokenIssuanceIDFromNode(node); issuanceID.has_value())
            issuanceIDs.insert(*issuanceID);
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
        key.size() == ripple::uint256::size(),
        "The size of the key is expected to fit uint256 exactly"
    );

    ripple::STLedgerEntry const sle = ripple::STLedgerEntry(
        ripple::SerialIter{blob.data(), blob.size()}, ripple::uint256::fromVoid(key.data())
    );

    if (sle.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltMPTOKEN)
        return {};

    auto const mptIssuanceID = sle[ripple::sfMPTokenIssuanceID];
    auto const holder = sle.getAccountID(ripple::sfAccount);

    return MPTHolderData{.mptID = mptIssuanceID, .holder = holder};
}

}  // namespace etl
