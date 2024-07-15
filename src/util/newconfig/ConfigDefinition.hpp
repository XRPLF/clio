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
#include "util/newconfig/ConfigFileInterface.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Errors.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/ValueView.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

namespace util::config {

class ArrayView;

/** @brief All the config description are stored and extracted from this class
 *
 * Represents all the possible config description
 */
struct ClioConfigDescription {
public:
    /** @brief Struct to represent a key-value pair*/
    struct KV {
        constexpr KV() = default;

        /** @brief Constructs a key value pair of config key and it's corresponding description
         *
         * @param k key of config
         * @param v value of config
         */
        constexpr KV(std::string_view k, std::string_view v) : key(k), value(v)
        {
        }
        std::string_view key;
        std::string_view value;
    };

    /** @brief Constructs a new Clio Config Description based on pre-existing descriptions
     *
     * @param init initializer list with a predefined set of key-value pairs where key is
     * config key and value is the description of the key
     */
    constexpr ClioConfigDescription(std::initializer_list<KV> init)
    {
        auto it = init.begin();
        for (std::size_t i = 0; i < configDescription.size(); ++i) {
            if (it != init.end()) {
                configDescription[i] = *it;
                ++it;
            }
        }
    }

    /**
     * @brief Retrieves the description for a given key
     *
     * @param key The key to look up the description for
     * @return The description associated with the key, or "Not Found" if the key does not exist
     */
    constexpr std::string_view
    get(std::string_view key) const
    {
        auto const itr = std::find_if(configDescription.begin(), configDescription.end(), [&key](auto const& v) {
            return v.key == key;
        });
        ASSERT(itr != configDescription.end(), "key doesn't exist in config");
        return itr->value;
    }

private:
    std::array<KV, 53> configDescription;
};

/** @brief All the config data will be stored and extracted from this class
 *
 * Represents all the possible config data
 */
class ClioConfigDefinition {
    friend class ObjectView;
    friend class ArrayView;
    friend class ValueView;

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
    std::optional<Error>
    getMarkDownFile(ClioConfigDescription const& configDescription) const;

    /**
     * @brief Returns the ObjectView specified with the prefix
     *
     * @param prefix The key prefix for the ObjectView
     * @param idx Optional index for the ObjectView
     * @return ObjectView with the given prefix
     * @throws std::runtime_error if no valid prefix exists in the config
     */
    ObjectView
    getObject(std::string_view prefix, std::optional<std::size_t> idx = std::nullopt) const;

    /**
     * @brief Returns the specified ValueView object associated with the key
     *
     * @param fullKey The config key to search for
     * @return ValueView associated with the given key
     * @throws std::runtime_error if no valid key exists in the config
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

private:
    std::unordered_map<std::string_view, std::variant<ConfigValue, Array>> map_;
};

}  // namespace util::config
