//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "data/DBHelpers.hpp"

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
// NOLINTNEXTLINE(misc-use-internal-linkage)
getMPTokenAuthorize(ripple::TxMeta const& txMeta)
{
    for (ripple::STObject const& node : txMeta.getNodes()) {
        if (node.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltMPTOKEN)
            continue;

        if (node.getFName() == ripple::sfCreatedNode) {
            auto const& newMPT = node.peekAtField(ripple::sfNewFields).downcast<ripple::STObject>();
            return MPTHolderData{
                .mptID = newMPT[ripple::sfMPTokenIssuanceID], .holder = newMPT.getAccountID(ripple::sfAccount)
            };
        }
    }
    return {};
}

std::optional<MPTHolderData>
// NOLINTNEXTLINE(misc-use-internal-linkage)
getMPTHolderFromTx(ripple::TxMeta const& txMeta, ripple::STTx const& sttx)
{
    if (txMeta.getResultTER() != ripple::tesSUCCESS || sttx.getTxnType() != ripple::TxType::ttMPTOKEN_AUTHORIZE)
        return {};

    return getMPTokenAuthorize(txMeta);
}

std::optional<MPTHolderData>
// NOLINTNEXTLINE(misc-use-internal-linkage)
getMPTHolderFromObj(std::string const& key, std::string const& blob)
{
    ripple::STLedgerEntry const sle =
        ripple::STLedgerEntry(ripple::SerialIter{blob.data(), blob.size()}, ripple::uint256::fromVoid(key.data()));

    if (sle.getFieldU16(ripple::sfLedgerEntryType) != ripple::ltMPTOKEN)
        return {};

    auto const mptIssuanceID = sle[ripple::sfMPTokenIssuanceID];
    auto const holder = sle.getAccountID(ripple::sfAccount);

    return MPTHolderData{.mptID = mptIssuanceID, .holder = holder};
}

}  // namespace etl
