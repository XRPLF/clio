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

#include "util/LedgerUtils.hpp"

#include <xrpl/protocol/LedgerFormats.h>

#include <algorithm>
#include <string>
#include <unordered_map>

namespace util {

ripple::LedgerEntryType
LedgerTypes::getLedgerEntryTypeFromStr(std::string const& entryName)
{
    static std::unordered_map<std::string, ripple::LedgerEntryType> kTYPE_MAP = []() {
        std::unordered_map<std::string, ripple::LedgerEntryType> map;
        std::ranges::for_each(kLEDGER_TYPES, [&map](auto const& item) { map[item.name_] = item.type_; });
        return map;
    }();

    if (kTYPE_MAP.find(entryName) == kTYPE_MAP.end())
        return ripple::ltANY;

    return kTYPE_MAP.at(entryName);
}

}  // namespace util
