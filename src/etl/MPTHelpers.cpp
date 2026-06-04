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
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

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
getMPTokenAuthorize(xrpl::TxMeta const& txMeta)
{
    for (xrpl::STObject const& node : txMeta.getNodes()) {
        if (node.getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltMPTOKEN)
            continue;

        if (node.getFName() == xrpl::sfCreatedNode) {
            auto const& newMPT = node.peekAtField(xrpl::sfNewFields).downcast<xrpl::STObject>();
            return MPTHolderData{
                .mptID = newMPT[xrpl::sfMPTokenIssuanceID], .holder = newMPT.getAccountID(xrpl::sfAccount)
            };
        }
    }
    return {};
}

std::optional<MPTHolderData>
getMPTHolderFromTx(xrpl::TxMeta const& txMeta, xrpl::STTx const& sttx)
{
    if (txMeta.getResultTER() != xrpl::tesSUCCESS || sttx.getTxnType() != xrpl::TxType::ttMPTOKEN_AUTHORIZE)
        return {};

    return getMPTokenAuthorize(txMeta);
}

std::optional<MPTHolderData>
getMPTHolderFromObj(std::string const& key, std::string const& blob)
{
    // https://github.com/XRPLF/XRPL-Standards/tree/master/XLS-0033-multi-purpose-tokens#2121-mptoken-ledger-identifier
    ASSERT(key.size() == xrpl::uint256::size(), "The size of the key is expected to fit uint256 exactly");

    xrpl::STLedgerEntry const sle =
        xrpl::STLedgerEntry(xrpl::SerialIter{blob.data(), blob.size()}, xrpl::uint256::fromVoid(key.data()));

    if (sle.getFieldU16(xrpl::sfLedgerEntryType) != xrpl::ltMPTOKEN)
        return {};

    auto const mptIssuanceID = sle[xrpl::sfMPTokenIssuanceID];
    auto const holder = sle.getAccountID(xrpl::sfAccount);

    return MPTHolderData{.mptID = mptIssuanceID, .holder = holder};
}

}  // namespace etl
