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

#include "util/JsonUtils.hpp"

#include <xrpl/protocol/TxFormats.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <unordered_set>

namespace util {

/**
 * @brief Get the transaction types in lowercase
 *
 * @return The transaction types in lowercase
 */
[[nodiscard]] std::unordered_set<std::string> const&
getTxTypesInLowercase()
{
    static std::unordered_set<std::string> const typesKeysInLowercase = []() {
        std::unordered_set<std::string> keys;
        std::transform(
            ripple::TxFormats::getInstance().begin(),
            ripple::TxFormats::getInstance().end(),
            std::inserter(keys, keys.begin()),
            [](auto const& pair) { return util::toLower(pair.getName()); }
        );
        return keys;
    }();

    return typesKeysInLowercase;
}
}  // namespace util
