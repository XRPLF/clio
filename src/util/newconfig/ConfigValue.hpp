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
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <variant>

namespace util::config {

template <typename>
constexpr bool always_false = false;

/** @brief Custom clio config types. */
enum class ConfigType { Integer, String, Double, Boolean };
=======
#include <string>
#include <type_traits>

namespace util::config {

/** @brief Custom clio config types. */
enum class ConfigType { Integer, String, Float, Boolean, Error };
>>>>>>> e62e648 (first draft of config)

/** @brief get the corresponding clio config type */
template <typename Type>
constexpr ConfigType
getType()
{
    if constexpr (std::is_same_v<Type, int>) {
        return ConfigType::Integer;
<<<<<<< HEAD
    } else if constexpr (std::is_same_v<Type, char const*>) {
        return ConfigType::String;
    } else if constexpr (std::is_same_v<Type, double>) {
        return ConfigType::Double;
    } else if constexpr (std::is_same_v<Type, bool>) {
        return ConfigType::Boolean;
    } else {
        static_assert(always_false<Type>, "Wrong config type");
    }
=======
    } else if constexpr (std::is_same_v<Type, std::string>) {
        return ConfigType::String;
    } else if constexpr (std::is_same_v<Type, float>) {
        return ConfigType::Float;
    } else if constexpr (std::is_same_v<Type, bool>) {
        return ConfigType::Boolean;
    }
    return ConfigType::Error;
>>>>>>> e62e648 (first draft of config)
}

/**
 * @brief Represents the config values for Json/Yaml config
 *
 * Used in ClioConfigDefinition to indicate the required type of value and
 * whether it is mandatory to specify in the configuration.
 */
<<<<<<< HEAD
class ConfigValue {
public:
    using Type = std::variant<int, char const*, bool, double>;

    constexpr ConfigValue() = default;
    constexpr ConfigValue(ConfigType type) : type_(type)
    {
    }

    constexpr ConfigValue&
    defaultValue(Type value)
    {
        setValue(value);
        return *this;
    }

=======
template <typename Type>
class ConfigValue {
public:
    constexpr ConfigValue(bool required) : type_{getType<Type>()}, required_{required}
    {
    }
>>>>>>> e62e648 (first draft of config)
    constexpr ConfigType
    type() const
    {
        return type_;
    }
<<<<<<< HEAD

    constexpr ConfigValue&
    required()
    {
        required_ = true;
        return *this;
    }

    std::string_view
    asString() const
    {
        if (type_ == ConfigType::String && value_.has_value())
            return std::get<char const*>(value_.value());
        throw std::bad_variant_access();
    }

    bool
    asBool() const
    {
        if (type_ == ConfigType::Boolean && value_.has_value())
            return std::get<bool>(value_.value());
        throw std::bad_variant_access();
    }

    int
    asInt() const
    {
        if (type_ == ConfigType::Integer && value_.has_value())
            return std::get<int>(value_.value());
        throw std::bad_variant_access();
    }

    double
    asDouble() const
    {
        if (type_ == ConfigType::Double && value_.has_value())
            return std::get<double>(value_.value());
        throw std::bad_variant_access();
    }

private:
    static void
    checkTypeConsistency(ConfigType type, Type const* value = nullptr)
    {
        if (value != nullptr) {
            if ((type == ConfigType::Integer && !std::holds_alternative<int>(*value)) ||
                (type == ConfigType::String && !std::holds_alternative<char const*>(*value)) ||
                (type == ConfigType::Double && !std::holds_alternative<double>(*value)) ||
                (type == ConfigType::Boolean && !std::holds_alternative<bool>(*value))) {
                throw std::invalid_argument("Type mismatch");
            }
        }
    }

    constexpr Type
    setValue(Type value)
    {
        checkTypeConsistency(type_, &value);
        value_ = value;
        return value;
    }

    ConfigType type_{};
    bool required_{false};
    std::optional<Type> value_;
=======
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
>>>>>>> e62e648 (first draft of config)
};

}  // namespace util::config
