#include "util/MPTIssuanceUtils.hpp"

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

namespace util {

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

bool
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

}  // namespace util
