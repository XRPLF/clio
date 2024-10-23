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
#include "util/newconfig/Types.hpp"

#include <fmt/core.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace util::config {

class ClioConfigDefinition;

/**
 * @brief Provides view into ConfigValues that represents values in Clio Config
 */
class ValueView {
public:
    /**
     * @brief Constructs a ValueView object
     *
     * @param configVal the config Value to view
     */
    ValueView(ConfigValue const& configVal);

    /**
     * @brief Retrieves the value as a string
     *
     * @return The value as a string
     * @throws std::bad_variant_access if the value is not a string
     */
    [[nodiscard]] std::string
    asString() const;

    /**
     * @brief Retrieves the value as a boolean
     *
     * @return The value as a boolean
     * @throws std::bad_variant_access if the value is not a boolean
     */
    [[nodiscard]] bool
    asBool() const;

    /**
     * @brief Retrieves any type of "int" value (uint_32, int64_t etc)
     *
     * @return The value with user requested type (if convertible)
     * @throws std::logic_error if the int < 0 and user requested unsigned int
     * @throws std::bad_variant_access if the value is not of type int
     */
    template <typename T>
    [[nodiscard]] T
    asIntType() const
    {
        if ((type() == ConfigType::Integer) && configVal_.get().hasValue()) {
            auto const val = std::get<int64_t>(configVal_.get().getValue());
            if (std::is_unsigned_v<T> && val < 0)
                ASSERT(false, "Int {} cannot be converted to the specified unsigned type", val);

            if (std::is_convertible_v<decltype(val), T>) {
                return static_cast<T>(val);
            }
        }
        ASSERT(false, "Value view is not of Int type");
        return 0;
    }

    /**
     * @brief Retrieves the value as a double
     *
     * @return The value as a double
     * @throws std::bad_variant_access if the value cannot be retrieved as a Double
     */
    [[nodiscard]] double
    asDouble() const;

    /**
     * @brief Retrieves the value as a float
     *
     * @return The value as a float
     * @throws std::bad_variant_access if the value cannot be retrieved as a float
     */
    [[nodiscard]] float
    asFloat() const;

    /**
     * @brief Gets the config type
     *
     * @return The config type
     */
    [[nodiscard]] constexpr ConfigType
    type() const
    {
        return configVal_.get().type();
    }

    /**
     * @brief Check if Config Value exists
     *
     * @return true if exists, false otherwise
     */
    [[nodiscard]] constexpr bool
    hasValue() const
    {
        return configVal_.get().hasValue();
    }

    /**
     * @brief Check if config value is optional
     *
     * @return true if optional, false otherwise
     */
    [[nodiscard]] constexpr bool
    isOptional() const
    {
        return configVal_.get().isOptional();
    }

    /**
     * @brief Retrieves the stored value as the specified type T
     *
     * @tparam T The type to cast the stored value to
     * @return The value cast to the specified type T
     */
    template <typename T>
    T
    getValueImpl() const
    {
        if constexpr (std::is_same_v<T, bool>) {
            ASSERT(type() == ConfigType::Boolean, "Value type is not a bool");
            return asBool();
        } else if constexpr (std::is_integral_v<T>) {
            ASSERT(type() == ConfigType::Integer, "Value type is not an int");
            return asIntType<T>();
        } else if constexpr (std::is_same_v<T, std::string>) {
            ASSERT(type() == ConfigType::String, "Value type is not a string");
            return asString();
        } else if constexpr (std::is_floating_point_v<T>) {
            ASSERT(type() == ConfigType::Double || type() == ConfigType::Integer, "Value type is not a floating point");
            return asDouble();
        }

        std::unreachable();
    }

    /**
     * @brief Returns an optional value of the specified type T if valid
     *
     * @tparam T The type of value to retrieve (must be compatible with internal type)
     * @return Returns the value as an optional<T> value exists, or std::nullopt if not
     */
    template <typename T>
    std::optional<T>
    asOptional() const
    {
        ASSERT(isOptional(), "Value is not an optional value");
        if (!hasValue())
            return std::nullopt;

        return std::make_optional(getValueImpl<T>());
    }

private:
    std::reference_wrapper<ConfigValue const> configVal_;
};

}  // namespace util::config
