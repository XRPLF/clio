#include "etl/MPTHelpers.hpp"

#include "data/DBHelpers.hpp"
#include "util/Assert.hpp"
#include "util/MPTIssuanceUtils.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
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

std::vector<MPTokenIssuanceTransactionsData>
getMPTokenIssuanceTxsFromTx(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx)
{
    // Collect each distinct issuance only once per transaction; the same set of affected accounts
    // is attached to every record produced below.
    util::MPTokenIssuanceIDs issuanceIDs;

    if (txMeta.getResultTER() == xrpl::tesSUCCESS) {
        for (auto const& node : txMeta.getNodes()) {
            if (auto const issuanceID = util::getMPTokenIssuanceIDFromNode(node);
                issuanceID.has_value()) {
                issuanceIDs.insert(*issuanceID);
            }
        }
    }

    util::addMPTokenIssuanceIDsFromTx(issuanceIDs, sttx);

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
