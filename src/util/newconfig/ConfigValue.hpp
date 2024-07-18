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

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace util::config {

template <typename>
constexpr bool alwaysFalse = false;

/** @brief Custom clio config types */
enum class ConfigType { UnsignedInt, Integer, String, Double, Boolean };

/**
 * @brief Get the corresponding clio config type
 *
 * @tparam Type The type to get the corresponding ConfigType for
 * @return The corresponding ConfigType
 */
template <typename Type>
constexpr ConfigType
getType()
{
    if constexpr (std::is_same_v<Type, int>) {
        return ConfigType::Integer;
    } else if constexpr (std::is_same_v<Type, uint32_t>) {
        return ConfigType::UnsignedInt;
    } else if constexpr (std::is_same_v<Type, std::string>) {
        return ConfigType::String;
    } else if constexpr (std::is_same_v<Type, double>) {
        return ConfigType::Double;
    } else if constexpr (std::is_same_v<Type, bool>) {
        return ConfigType::Boolean;
    } else {
        static_assert(alwaysFalse<Type>, "Wrong config type");
    }
}

/**
 * @brief Represents the config values for Json/Yaml config
 *
 * Used in ClioConfigDefinition to indicate the required type of value and
 * whether it is mandatory to specify in the configuration
 */
class ConfigValue {
    friend class ValueView;

public:
    using Type = std::variant<uint32_t, int64_t, std::string, bool, double>;

    /**
     * @brief Constructor initializing with the config type
     *
     * @param type The type of the config value
     */
    constexpr ConfigValue(ConfigType type) : type_(type)
    {
    }

    /**
     * @brief Sets the default value for the config
     *
     * @param value The default value
     * @return Reference to this ConfigValue
     */
    ConfigValue&
    defaultValue(Type value)
    {
        required_ = false;
        setValue(value);
        return *this;
    }

    /**
     * @brief Gets the config type
     *
     * @return The config type
     */
    constexpr ConfigType
    type() const
    {
        return type_;
    }

    /**
     * @brief Sets the minimum value for the config
     *
     * @param min The minimum value
     * @return Reference to this ConfigValue
     */
    constexpr ConfigValue&
    min(std::uint32_t min)
    {
        min_ = min;
        return *this;
    }

    /**
     * @brief Sets the maximum value for the config
     *
     * @param max The maximum value
     * @return Reference to this ConfigValue
     */
    constexpr ConfigValue&
    max(std::uint32_t max)
    {
        max_ = max;
        return *this;
    }

    /**
     * @brief Sets the config value as optional, meaning the user doesn't have to provide the value in their config
     *
     * @return Reference to this ConfigValue
     */
    constexpr ConfigValue&
    optional()
    {
        required_ = false;
        ASSERT(!value_.has_value(), "value must not exist in optional");
        return *this;
    }

private:
    /**
     * @brief Checks if the value type is consistent with the specified ConfigType
     *
     * @param type The config type
     * @param value The config value
     */
    static void
    checkTypeConsistency(ConfigType type, Type value)
    {
        if (std::holds_alternative<std::string>(value)) {
            ASSERT(type == ConfigType::String, "Value does not match type string");
        } else if (std::holds_alternative<bool>(value)) {
            ASSERT(type == ConfigType::Boolean, "Value does not match type boolean");
        } else if (std::holds_alternative<double>(value)) {
            ASSERT(type == ConfigType::Double, "Value does not match type double");
        } else if (std::holds_alternative<uint32_t>(value)) {
            ASSERT(type == ConfigType::UnsignedInt, "Value does not match type unsigned int");
        } else if (std::holds_alternative<int64_t>(value)) {
            ASSERT(type == ConfigType::Integer, "Value does not match type integer");
        }
    }

    /**
     * @brief Sets the value for the config
     *
     * @param value The value to set
     * @return The value that was set
     */
    Type
    setValue(Type value)
    {
        checkTypeConsistency(type_, value);
        value_ = value;
        return value;
    }

    ConfigType type_{};
    bool required_{true};
    std::optional<Type> value_;
    std::optional<std::uint32_t> min_;
    std::optional<std::uint32_t> max_;
};

}  // namespace util::config
