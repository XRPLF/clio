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

#include <string>
#include <type_traits>

namespace util::config {

/** @brief Custom clio config types. */
enum class ConfigType { Integer, String, Float, Boolean, Error };

/** @brief get the corresponding clio config type */
template <typename Type>
constexpr ConfigType
getType()
{
    if constexpr (std::is_same_v<Type, int>) {
        return ConfigType::Integer;
    } else if constexpr (std::is_same_v<Type, std::string>) {
        return ConfigType::String;
    } else if constexpr (std::is_same_v<Type, float>) {
        return ConfigType::Float;
    } else if constexpr (std::is_same_v<Type, bool>) {
        return ConfigType::Boolean;
    }
    return ConfigType::Error;
}

/**
 * @brief Represents the config values for Json/Yaml config
 *
 * Used in ClioConfigDefinition to indicate the required type of value and
 * whether it is mandatory to specify in the configuration.
 */
template <typename Type>
class ConfigValue {
public:
    constexpr ConfigValue(bool required) : type_{getType<Type>()}, required_{required}
    {
    }
    constexpr ConfigType
    type() const
    {
        return type_;
    }
    constexpr bool
    required() const
    {
        return required_;
    }

private:
    ConfigType type_;
    bool required_;
};

/**
 * @brief Represents the user's value for Json/Yaml config
 *
 */

// don't know if this can be templated?
template <typename Type>
class ValueData {
public:
    ValueData(Type value) : type_{getType<Type>()}, value_{value}
    {
    }
    ConfigType
    type() const
    {
        return type_;
    }
    Type
    value() const
    {
        return value_;
    }

private:
    ConfigType type_;
    Type value_;
};

}  // namespace util::config
