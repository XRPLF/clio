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
#include "util/OverloadSet.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/Error.hpp"
#include "util/newconfig/Types.hpp"

#include <fmt/core.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace util::config {

/**
 * @brief Represents the config values for Json/Yaml config
 *
 * Used in ClioConfigDefinition to indicate the required type of value and
 * whether it is mandatory to specify in the configuration
 */
class ConfigValue {
public:
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
    [[nodiscard]] ConfigValue&
    defaultValue(Value value)
    {
        auto const err = checkTypeConsistency(type_, value);
        ASSERT(!err.has_value(), "{}", err->error);
        value_ = value;
        return *this;
    }

    /**
     * @brief Sets the value current ConfigValue given by the User's defined value
     *
     * @param value The value to set
     * @param key The Config key associated with the value. Optional to include; Used for debugging message to user.
     * @return optional Error if user tries to set a value of wrong type or not within a constraint
     */
    [[nodiscard]] std::optional<Error>
    setValue(Value value, std::optional<std::string_view> key = std::nullopt)
    {
        auto err = checkTypeConsistency(type_, value);
        if (err.has_value()) {
            if (key.has_value())
                err->error = fmt::format("{} {}", key.value(), err->error);
            return err;
        }

        if (cons_.has_value()) {
            auto constraintCheck = cons_->get().checkConstraint(value);
            if (constraintCheck.has_value()) {
                if (key.has_value())
                    constraintCheck->error = fmt::format("{} {}", key.value(), constraintCheck->error);
                return constraintCheck;
            }
        }
        value_ = value;
        return std::nullopt;
    }

    /**
     * @brief Assigns a constraint to the ConfigValue.
     *
     * This method associates a specific constraint with the ConfigValue.
     * If the ConfigValue already holds a value, the method will check whether
     * the value satisfies the given constraint. If the constraint is not satisfied,
     * an assertion failure will occur with a detailed error message.
     *
     * @param cons The constraint to be applied to the ConfigValue.
     * @return A reference to the modified ConfigValue object.
     */
    [[nodiscard]] constexpr ConfigValue&
    withConstraint(Constraint const& cons)
    {
        cons_ = std::reference_wrapper<Constraint const>(cons);
        ASSERT(cons_.has_value(), "Constraint must be defined");

        if (value_.has_value()) {
            auto const& temp = cons_.value().get();
            auto const& result = temp.checkConstraint(value_.value());
            if (result.has_value()) {
                // useful for specifying clear Error message
                std::string type;
                std::visit(
                    util::OverloadSet{
                        [&type](bool tmp) { type = fmt::format("bool {}", tmp); },
                        [&type](std::string const& tmp) { type = fmt::format("string {}", tmp); },
                        [&type](double tmp) { type = fmt::format("double {}", tmp); },
                        [&type](int64_t tmp) { type = fmt::format("int {}", tmp); }
                    },
                    value_.value()
                );
                ASSERT(false, "Value {} ConfigValue does not satisfy the set Constraint", type);
            }
        }
        return *this;
    }

    /**
     * @brief Retrieves the constraint associated with this ConfigValue, if any.
     *
     * @return An optional reference to the associated Constraint.
     */
    [[nodiscard]] std::optional<std::reference_wrapper<Constraint const>>
    getConstraint() const
    {
        return cons_;
    }

    /**
     * @brief Gets the config type
     *
     * @return The config type
     */
    [[nodiscard]] constexpr ConfigType
    type() const
    {
        return type_;
    }

    /**
     * @brief Sets the config value as optional, meaning the user doesn't have to provide the value in their config
     *
     * @return Reference to this ConfigValue
     */
    [[nodiscard]] constexpr ConfigValue&
    optional()
    {
        optional_ = true;
        return *this;
    }

    /**
     * @brief Checks if configValue is optional
     *
     * @return true if optional, false otherwise
     */
    [[nodiscard]] constexpr bool
    isOptional() const
    {
        return optional_;
    }

    /**
     * @brief Check if value is optional
     *
     * @return if value is optiona, false otherwise
     */
    [[nodiscard]] constexpr bool
    hasValue() const
    {
        return value_.has_value();
    }

    /**
     * @brief Get the value of config
     *
     * @return Config Value
     */
    [[nodiscard]] Value const&
    getValue() const
    {
        return value_.value();
    }

private:
    /**
     * @brief Checks if the value type is consistent with the specified ConfigType
     *
     * @param type The config type
     * @param value The config value
     */
    static std::optional<Error>
    checkTypeConsistency(ConfigType type, Value value)
    {
        if (type == ConfigType::String && !std::holds_alternative<std::string>(value)) {
            return Error{"value does not match type string"};
        }
        if (type == ConfigType::Boolean && !std::holds_alternative<bool>(value)) {
            return Error{"value does not match type boolean"};
        }
        if (type == ConfigType::Double && !std::holds_alternative<double>(value)) {
            return Error{"value does not match type double"};
        }
        if (type == ConfigType::Integer && !std::holds_alternative<int64_t>(value)) {
            return Error{"value does not match type integer"};
        }
        return std::nullopt;
    }

    ConfigType type_{};
    bool optional_{false};
    std::optional<Value> value_;
    std::optional<std::reference_wrapper<Constraint const>> cons_;
};

}  // namespace util::config
