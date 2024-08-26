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
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Errors.hpp"
#include "util/newconfig/Types.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace util::config {

/**
 * @brief Array definition to store multiple values provided by the user from Json/Yaml
 *
 * Used in ClioConfigDefinition to represent multiple potential values (like whitelist)
 * Is constructed with only 1 element which states which type/constraint must every element
 * In the array satisfy
 */
class Array {
public:
    /**
     * @brief Constructs an Array with provided Arg
     *
     * @tparam Args Types of the arguments
     * @param arg Argument to set the type and constraint of ConfigValues in Array
     */
    Array(ConfigValue arg) : elements_{std::move(arg)}
    {
        ASSERT(!elements_.at(0).hasValue(), "Array does not include default values");
    }

    /**
     * @brief Add ConfigValues to Array class
     *
     * @param value The ConfigValue to add
     * @param key optional string key to include that will show in error message
     * @return optional error if adding config value to array fails. nullopt otherwise
     */
    std::optional<Error>
    addValue(Value value, std::optional<std::string_view> key = std::nullopt);

    /**
     * @brief Returns the number of values stored in the Array
     *
     * @return Number of values stored in the Array
     */
    [[nodiscard]] size_t
    size() const;

    /**
     * @brief Returns the ConfigValue at the specified index
     *
     * @param idx Index of the ConfigValue to retrieve
     * @return ConfigValue at the specified index
     */
    [[nodiscard]] ConfigValue const&
    at(std::size_t idx) const;

    /**
     * @brief Returns an iterator to the beginning of the ConfigValue vector.
     *
     * @return A constant iterator to the beginning of the vector.
     */
    [[nodiscard]] std::vector<ConfigValue>::const_iterator
    begin() const;

    /**
     * @brief Returns an iterator to the end of the ConfigValue vector.
     *
     * @return A constant iterator to the end of the vector.
     */
    [[nodiscard]] std::vector<ConfigValue>::const_iterator
    end() const;

private:
    std::vector<ConfigValue> elements_;
};

}  // namespace util::config
