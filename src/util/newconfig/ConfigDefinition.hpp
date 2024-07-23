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

#include <__expected/expected.h>
#include <fmt/core.h>

#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

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
    ClioConfigDefinition(std::initializer_list<KeyValuePair> pair)
    {
        for (auto const& p : pair) {
            if (p.first.contains("[]"))
                ASSERT(std::holds_alternative<Array>(p.second), "value must be array if key has \"[]\"");
            map_.insert(p);
        }
    }

    /**
     * @brief Parses the configuration file
     *
     * Should also check that no extra configuration key/value pairs are present
     *
     * @param config The configuration file interface
     * @return An optional Error object if parsing fails
     */
    std::optional<Error>
    parse(ConfigFileInterface const& config);

    /**
     * @brief Validates the configuration file
     *
     * Should only check for valid values, without populating
     *
     * @param config The configuration file interface
     * @return An optional Error object if validation fails
     */
    std::optional<Error>
    validate(ConfigFileInterface const& config) const;

    /**
     * @brief Generate markdown file of all the clio config descriptions
     *
     * @param configDescription The configuration description object
     * @return An optional Error if generating markdown fails
     */
    std::expected<std::string, Error>
    getMarkDown(ClioConfigDescription const& configDescription) const;

    /**
     * @brief Returns the ObjectView specified with the prefix
     *
     * @param prefix The key prefix for the ObjectView
     * @return ObjectView with the given prefix
     */
    ObjectView
    getObject(std::string_view prefix, std::optional<std::size_t> idx = std::nullopt) const;

    /**
     * @brief Returns the specified ValueView object associated with the key
     *
     * @param fullKey The config key to search for
     * @return ValueView associated with the given key
     */
    ValueView
    getValue(std::string_view fullKey) const;

    /**
     * @brief Returns the specified Array object from ClioConfigDefinition
     *
     * @param prefix The prefix to search config keys from
     * @return ArrayView with all key-value pairs where key starts with "prefix"
     */
    ArrayView
    getArray(std::string_view prefix) const;

    /**
     * @brief Returns an iterator to the beginning of the configuration map.
     *
     * @return A constant iterator to the beginning of the map.
     */
    auto
    begin() const
    {
        return map_.begin();
    }

    /**
     * @brief Returns an iterator to the end of the configuration map.
     *
     * @return A constant iterator to the end of the map.
     */
    auto
    end() const
    {
        return map_.end();
    }

    /**
     * @brief Checks if a key is present in the configuration map.
     *
     * @param key The key to search for in the configuration map.
     * @return True if the key is present, false otherwise.
     */
    bool
    contains(std::string_view key) const
    {
        return map_.contains(key);
    }

    /**
     * @brief Returns the Array object associated with the specified key.
     *
     * @param key The key whose associated Array object is to be returned.
     * @return The Array object associated with the specified key.
     */
    Array const&
    atArray(std::string_view key) const;

    /**
     * @brief Returns the size of an Array
     *
     * @param prefix The prefix whose associated Array object is to be returned.
     * @return The size of the array associated with the specified prefix.
     */
    std::size_t
    arraySize(std::string_view prefix) const;

private:
    std::unordered_map<std::string_view, std::variant<ConfigValue, Array>> map_;
};

}  // namespace util::config
