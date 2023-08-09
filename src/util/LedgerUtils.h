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

#include <ripple/basics/Slice.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/LedgerHeader.h>

#include <sstream>
#include <string>

namespace clio::util {

inline ripple::LedgerHeader
deserializeHeader(ripple::Slice data)
{
    return ripple::deserializeHeader(data, /*hasHash=*/true);
}

inline std::string
toString(ripple::LedgerHeader const& info)
{
    std::stringstream ss;
    ss << "LedgerHeader { Sequence : " << info.seq << " Hash : " << ripple::strHex(info.hash)
       << " TxHash : " << strHex(info.txHash) << " AccountHash : " << ripple::strHex(info.accountHash)
       << " ParentHash : " << strHex(info.parentHash) << " }";
    return ss.str();
}

}  // namespace clio::util
