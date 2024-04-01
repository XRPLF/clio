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

#include "util/StringUtils.hpp"

#include <ripple/protocol/LedgerHeader.h>

#include <string>

std::string
hexStringToBinaryString(std::string const& hex)
{
    auto const blob = ripple::strUnHex(hex);
    std::string strBlob;

    for (auto c : *blob)
        strBlob += c;

    return strBlob;
}

ripple::uint256
binaryStringToUint256(std::string const& bin)
{
    return ripple::uint256::fromVoid(static_cast<void const*>(bin.data()));
}

std::string
ledgerInfoToBinaryString(ripple::LedgerInfo const& info)
{
    auto const blob = rpc::ledgerInfoToBlob(info, true);
    std::string strBlob;
    for (auto c : blob)
        strBlob += c;

    return strBlob;
};
