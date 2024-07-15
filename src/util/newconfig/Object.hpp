<<<<<<< HEAD
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

#pragma once

<<<<<<< HEAD
#include "util/newconfig/ConfigValue.hpp"

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
=======
#include <tuple>
>>>>>>> e62e648 (first draft of config)

namespace util::config {

/**
 * @brief Object definition for Json/Yaml config
 *
 * Used in ClioConfigDefinition to represent key-value(s) pair.
 */
<<<<<<< HEAD
/*
template <typename Key, typename Value, std::size_t Size>
struct Map {
   using Item = std::pair<Key, Value>;
   std::array<Item, Size> data;

   constexpr Map(std::initializer_list<Item> init_list)
   {
       if (init_list.size() != Size) {
           throw std::invalid_argument("Initializer list size does not match array size");
       }
       std::copy(init_list.begin(), init_list.end(), data.begin());
   }

   [[nodiscard]] constexpr Value
   at(Key const& key) const
   {
       auto const itr = std::find_if(begin(data), end(data), [&key](auto const& v) { return v.first == key; });
       if (itr != end(data)) {
           return itr->second;
       }
       throw std::range_error("Not Found");
   }

   constexpr std::size_t
   size() const
   {
       return data.size();
   }

   constexpr size_t
   countWithPrefix(std::string_view s) const
   {
       return std::ranges::count_if(data, [&s](Item const& i) { return i.first.starts_with(s); });
   }

   [[nodiscard]] std::vector<Item>
   withPrefix(std::string_view subStr, std::size_t configSize) const;
}; */

class Object {
    // A Predetermined number of Clio Configs
    static int const configSize = 24U;

public:
    using KeyValuePair = std::pair<std::string_view, ConfigValue>;
    // constexpr Object(std::initializer_list<KeyValuePair> pair) : map_{pair}

    Object(std::initializer_list<KeyValuePair> pair)
    {
        for (auto const& p : pair)
            map_.insert(p);
    }

    size_t
    countWithPrefix(std::string_view prefix) const
    {
        return std::count_if(map_.begin(), map_.end(), [&prefix](auto const& pair) {
            return pair.first.starts_with(prefix);
        });
    }

    /** @brief return the specified Array key-values with prefix "key"
     *
     * Returns an array with all key-value pairs where key starts with "prefix"
     */
    std::vector<KeyValuePair>
    getArray(std::string_view prefix) const;

    /** @brief return the specified value associated with key
     *
     * @param key The config key to search for
     * @return Returns config value associated with the given key.
     * @throws if no valid key exists in config
     */
    constexpr ConfigValue
    getValue(std::string_view key) const
    {
        if (map_.contains(key))
            return map_.at(key);
        throw std::invalid_argument("no matching key");
    }

    void
    printMap()
    {
        for (auto& pair : map_) {
            std::cout << pair.first << " " << std::endl;
        }
    }

private:
    std::unordered_map<std::string_view, ConfigValue> map_;
    // Map<std::string_view, ConfigValue, configSize> map_;
=======
template <typename Key, typename... Args>
class Object {
public:
    constexpr Object(Key key, Args... args) : key_{key}, fields_{args...}
    {
    }
    constexpr Key&
    key() const
    {
        return key_;
    }
    constexpr std::tuple<Args...>&
    Val() const
    {
        return fields_;
    }

private:
    Key key_;
    std::tuple<Args...> fields_{};
>>>>>>> e62e648 (first draft of config)
};

}  // namespace util::config
=======
>>>>>>> d2f765f (Commit work so far)
