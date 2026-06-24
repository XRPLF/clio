#include "data/DBHelpers.hpp"
#include "util/Assert.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STBase.h>
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
        return {};

    auto const mptIssuanceID = sle[xrpl::sfMPTokenIssuanceID];
    auto const holder = sle.getAccountID(xrpl::sfAccount);

    return MPTHolderData{.mptID = mptIssuanceID, .holder = holder};
}

}  // namespace etl
