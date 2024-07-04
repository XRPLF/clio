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

#include "util/newconfig/Object.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace util::config {

/*
template <typename Key, typename Value, std::size_t Size>
[[nodiscard]] std::vector<typename Map<Key, Value, Size>::Item>
Map<Key, Value, Size>::withPrefix(std::string_view subStr, std::size_t configSize) const
{
    std::vector<Item> result{};
    result.reserve(configSize);
    auto it_result = result.begin();

    for (auto it = begin(data); it != end(data); ++it) {
        if (it->first.substr(0, subStr.size()) == subStr) {
            *it_result++ = *it;
        }
    }
    return result;
} */

std::vector<Object::KeyValuePair>
Object::getArray(std::string_view prefix) const
{
    std::vector<KeyValuePair> arr;
    for (auto const& [key, value] : map_) {
        if (key.starts_with(prefix)) {
            arr.emplace_back(key, value);
        }
    }
    return arr;
}

}  // namespace util::config
