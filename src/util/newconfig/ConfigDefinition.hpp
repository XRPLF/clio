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

#include "util/Assert.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigDescription.hpp"
#include "util/newconfig/ConfigFileInterface.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Errors.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/ValueView.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <expected>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace util::config {

/**
 * @brief All the config data will be stored and extracted from this class
 *
 * Represents all the possible config data
 */
class ClioConfigDefinition {
public:
    using KeyValuePair = std::pair<std::string_view, std::variant<ConfigValue, Array>>;

    /**
     * @brief Constructs a new ClioConfigDefinition
     *
     * Initializes the configuration with a predefined set of key-value pairs
     * If a key contains "[]", the corresponding value must be an Array
     *
     * @param pair A list of key-value pairs for the predefined set of clio configurations
     */
    ClioConfigDefinition(std::initializer_list<KeyValuePair> pair);

    /**
     * @brief Parses the configuration file
     *
     * Should also check that no extra configuration key/value pairs are present
     *
     * @param config The configuration file interface
     * @return An optional vector of Error objects stating all the failures if parsing fails
     */
    [[nodiscard]] std::optional<std::vector<Error>>
    parse(ConfigFileInterface const& config);

    /**
     * @brief Validates the configuration file
     *
     * Should only check for valid values, without populating
     *
     * @param config The configuration file interface
     * @return An optional vector of Error objects stating all the failures if validation fails
     */
    [[nodiscard]] std::optional<std::vector<Error>>
    validate(ConfigFileInterface const& config) const;

    /**
     * @brief Generate markdown file of all the clio config descriptions
     *
     * @param configDescription The configuration description object
     * @return An optional Error if generating markdown fails
     */
    [[nodiscard]] std::expected<std::string, Error>
    getMarkdown(ClioConfigDescription const& configDescription) const;

    /**
     * @brief Returns the ObjectView specified with the prefix
     *
     * @param prefix The key prefix for the ObjectView
     * @param idx Used if getting Object in an Array
     * @return ObjectView with the given prefix
     */
    [[nodiscard]] ObjectView
    getObject(std::string_view prefix, std::optional<std::size_t> idx = std::nullopt) const;

    /**
     * @brief Returns the specified ValueView object associated with the key
     *
     * @param fullKey The config key to search for
     * @return ValueView associated with the given key
     */
    [[nodiscard]] ValueView
    getValue(std::string_view fullKey) const;

    /**
     * @brief Returns the specified ValueView object in an array with a given index
     *
     * @param fullKey The config key to search for
     * @param index The index of the config value inside the Array to get
     * @return ValueView associated with the given key
     */
    [[nodiscard]] ValueView
    getValueInArray(std::string_view fullKey, std::size_t index) const;

    /**
     * @brief Returns the specified Array object from ClioConfigDefinition
     *
     * @param prefix The prefix to search config keys from
     * @return ArrayView with all key-value pairs where key starts with "prefix"
     */
    [[nodiscard]] ArrayView
    getArray(std::string_view prefix) const;

    /**
     * @brief Checks if a key is present in the configuration map.
     *
     * @param key The key to search for in the configuration map.
     * @return True if the key is present, false otherwise.
     */
    [[nodiscard]] bool
    contains(std::string_view key) const;

    /**
     * @brief Checks if any key in config starts with "key"
     *
     * @param key The key to search for in the configuration map.
     * @return True if the any key in config starts with "key", false otherwise.
     */
    [[nodiscard]] bool
    hasItemsWithPrefix(std::string_view key) const;

    /**
     * @brief Returns the Array object associated with the specified key.
     *
     * @param key The key whose associated Array object is to be returned.
     * @return The Array object associated with the specified key.
     */
    [[nodiscard]] Array const&
    asArray(std::string_view key) const;

    /**
     * @brief Returns the size of an Array
     *
     * @param prefix The prefix whose associated Array object is to be returned.
     * @return The size of the array associated with the specified prefix.
     */
    [[nodiscard]] std::size_t
    arraySize(std::string_view prefix) const;

    /**
     * @brief Returns an iterator to the beginning of the configuration map.
     *
     * @return A constant iterator to the beginning of the map.
     */
    [[nodiscard]] auto
    begin() const
    {
        return map_.begin();
    }

    /**
     * @brief Returns an iterator to the end of the configuration map.
     *
     * @return A constant iterator to the end of the map.
     */
    [[nodiscard]] auto
    end() const
    {
        return map_.end();
    }

private:
    /**
     * @brief returns the iterator of key-value pair with key fullKey
     *
     * @param fullKey Key to search for
     * @return iterator of map
     */
    [[nodiscard]] auto
    getArrayIterator(std::string_view key) const
    {
        auto const fullKey = addBracketsForArrayKey(key);
        auto const it = std::ranges::find_if(map_, [&fullKey](auto pair) { return pair.first == fullKey; });

        ASSERT(it != map_.end(), "key {} does not exist in config", fullKey);
        ASSERT(std::holds_alternative<Array>(it->second), "Value of {} is not an array", fullKey);

        return it;
    }

    /**
     * @brief Used for all Array API's; check to make sure "[]" exists, if not, append to end
     *
     * @param key key to check for
     * @return the key with "[]" appended to the end
     */
    [[nodiscard]] static std::string
    addBracketsForArrayKey(std::string_view key)
    {
        std::string fullKey = std::string(key);
        if (!key.contains(".[]"))
            fullKey += ".[]";
        return fullKey;
    }

    std::unordered_map<std::string_view, std::variant<ConfigValue, Array>> map_;
};

}  // namespace util::config
