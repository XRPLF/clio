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

#include "util/JsonUtils.hpp"

#include <boost/algorithm/string.hpp>
#include <xrpl/protocol/LedgerFormats.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace util {

ripple::LedgerEntryType
LedgerTypes::getLedgerEntryTypeFromStr(std::string const& entryName)
{
    if (auto const result = getLedgerTypeAttributeFromStr(entryName); result.has_value()) {
        return result->get().type_;
    }
    return ripple::ltANY;
}

ripple::LedgerEntryType
LedgerTypes::getAccountOwnedLedgerTypeFromStr(std::string const& entryName)
{
    if (auto const result = getLedgerTypeAttributeFromStr(entryName);
        result.has_value() && result->get().category_ != LedgerTypeAttribute::LedgerCategory::Chain) {
        return result->get().type_;
    }

    return ripple::ltANY;
}

std::optional<std::reference_wrapper<impl::LedgerTypeAttribute const>>
LedgerTypes::getLedgerTypeAttributeFromStr(std::string const& entryName)
{
    static std::unordered_map<std::string, std::reference_wrapper<impl::LedgerTypeAttribute const>> const kNAME_MAP =
        []() {
            std::unordered_map<std::string, std::reference_wrapper<impl::LedgerTypeAttribute const>> map;
            std::ranges::for_each(kLEDGER_TYPES, [&map](auto const& item) {
                map.insert({util::toLower(item.name_), item});
            });
            return map;
        }();

    static std::unordered_map<std::string, std::reference_wrapper<impl::LedgerTypeAttribute const>> const
        kRPC_NAME_MAP = []() {
            std::unordered_map<std::string, std::reference_wrapper<impl::LedgerTypeAttribute const>> map;
            std::ranges::for_each(kLEDGER_TYPES, [&map](auto const& item) { map.insert({item.rpcName_, item}); });
            return map;
        }();

    if (auto const it = kRPC_NAME_MAP.find(entryName); it != kRPC_NAME_MAP.end()) {
        return it->second;
    }

    auto const entryNameLowercase = util::toLower(entryName);
    if (auto const it = kNAME_MAP.find(entryNameLowercase); it != kNAME_MAP.end()) {
        return it->second;
    }

    return std::nullopt;
}

}  // namespace util
