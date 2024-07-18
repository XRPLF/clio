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

#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/ValueView.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace util::config {

class ClioConfigDefinition;
class ArrayView;

/**
 * @brief Provides a view into a subset of configuration data defined by a prefix
 *
 * Allows querying and accessing configuration values based on the provided prefix
 */
class ObjectView {
    friend class ClioConfigDefinition;

public:
    /**
     * @brief Constructs an ObjectView for the specified prefix. The view must be of type object
     *
     * @param prefix The prefix indicating the subset of configuration data to view
     * @param clioConfig Reference to the ClioConfigDefinition containing all the configuration data
     */
    ObjectView(std::string_view prefix, ClioConfigDefinition const& clioConfig);

    /**
     * @brief Constructs an ObjectView for an indexed array within the specified prefix
     *
     * @param prefix The prefix indicating the subset of configuration data to view
     * @param arrayIndex The index of the array object element to view
     * @param clioConfig Reference to the ClioConfigDefinition containing all the configuration data
     */
    ObjectView(std::string_view prefix, std::size_t arrayIndex, ClioConfigDefinition const& clioConfig);

    /**
     * @brief Checks if prefix_.key (fullkey) exists in ClioConfigDefinition
     *
     * @param key The suffix of the key
     * @return true if the full key exists, otherwise false
     */
    [[nodiscard]] bool
    containsKey(std::string_view key) const;

    /**
     * @brief Retrieves the value associated with the specified prefix._key in ClioConfigDefinition
     *
     * @param key The suffix of the key
     * @return A ValueView object representing the value associated with the key
     */
    [[nodiscard]] ValueView
    getValue(std::string_view key) const;

    /**
     * @brief Retrieves an ObjectView in ClioConfigDefinition with key that starts with prefix_.key. The view must be of
     * type object
     *
     * @param key The suffix of the key
     * @return An ObjectView representing the subset of configuration data
     */
    [[nodiscard]] ObjectView
    getObject(std::string_view key) const;

private:
    /**
     * @brief returns the full key (prefix.key)
     *
     * @param key The suffix of the key
     * @return the full string of the key
     */
    [[nodiscard]] std::string
    getFullKey(std::string_view key) const;

    /**
     * @brief Checks if any key in ClioConfigDefinition starts with prefix_.key
     *
     * @param key The suffix of the key
     * @return true if at least one key starts with the specified prefix_.key, otherwise false
     */
    [[nodiscard]] bool
    startsWithKey(std::string_view key) const;

    std::string prefix_;
    std::optional<size_t> arrayIndex_;
    std::reference_wrapper<ClioConfigDefinition const> clioConfig_;
};

}  // namespace util::config
