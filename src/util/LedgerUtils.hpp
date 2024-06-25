//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#pragma once

#include <fmt/core.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <string>
#include <unordered_set>
#include <vector>

namespace util {

/**
 * @brief Returns a string set of all supported ledger entry types
 *
 * @return The set of ledger entry types
 */
std::unordered_set<std::string> const&
getLedgerEntryTypeStrs();

/**
 * @brief Return the ledger type from a string representation
 *
 * @param entryName The string representation of the ledger entry type
 * @return The ledger entry type
 */
ripple::LedgerEntryType
getLedgerEntryTypeFromStr(std::string const& entryName);

/**
 * @brief Return the list of ledger entry types which will block the account deletion
 *
 * @return The list of ledger entry types
 */
std::vector<ripple::LedgerEntryType> const&
getDeletionBlockerLedgerTypes();

/**
 * @brief Deserializes a ripple::LedgerHeader from ripple::Slice of data.
 *
 * @param data The slice to deserialize
 * @return The deserialized ripple::LedgerHeader
 */
inline ripple::LedgerHeader
deserializeHeader(ripple::Slice data)
{
    return ripple::deserializeHeader(data, /* hasHash = */ true);
}

/**
 * @brief A helper function that converts a ripple::LedgerHeader to a string representation.
 *
 * @param info The ledger header
 * @return The string representation of the supplied ledger header
 */
inline std::string
toString(ripple::LedgerHeader const& info)
{
    return fmt::format(
        "LedgerHeader {{Sequence: {}, Hash: {}, TxHash: {}, AccountHash: {}, ParentHash: {}}}",
        info.seq,
        ripple::strHex(info.hash),
        strHex(info.txHash),
        ripple::strHex(info.accountHash),
        strHex(info.parentHash)
    );
}

}  // namespace util
