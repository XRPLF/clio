#include "data/DBHelpers.hpp"
#include "util/Assert.hpp"

#include <ripple/protocol/STBase.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>

#include <optional>
#include <string>

namespace etl {

/**
 * @brief Get the MPToken created from a transaction
 *
 * @param txMeta Transaction metadata
 * @return MPT and holder account pair
 */
std::optional<MPTHolderData>
getMPTokenAuthorize(ripple::TxMeta const& txMeta)
{
    for (ripple::STObject const& node : txMeta.getNodes()) {
        if (node.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltMPTOKEN)
            continue;

        if (node.getFName() == ripple::sfCreatedNode) {
            auto const& newMPT = node.peekAtField(ripple::sfNewFields).downcast<ripple::STObject>();
            return MPTHolderData{
                .mptID = newMPT[ripple::sfMPTokenIssuanceID],
                .holder = newMPT.getAccountID(ripple::sfAccount)
            };
        }
    }
    return {};
}

std::optional<MPTHolderData>
getMPTHolderFromTx(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    if (txMeta.getResultTER() != ripple::tesSUCCESS ||
        sttx.getTxnType() != ripple::TxType::ttMPTOKEN_AUTHORIZE)
        return {};

    return getMPTokenAuthorize(txMeta);
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
