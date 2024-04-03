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

#include "rpc/Errors.hpp"
#include "rpc/common/ValidationHelpers.hpp"

#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/core.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rpc::check {
/**
 * @brief Warning that checks can return
 */
struct Warning {
    /**
     * @brief Construct a new Warning object
     *
     * @param code The warning code
     * @param message The warning message
     */
    Warning(WarningCode code, std::string message) : warningCode(code), extraMessage(std::move(message))
    {
    }

    bool
    operator==(Warning const& other) const = default;

    WarningCode warningCode;
    std::string extraMessage;
};
using Warnings = std::vector<Warning>;

/**
 * @brief Check for a deprecated fields
 */
template <typename... T>
class Deprecated;

/**
 * @brief Check if a field is deprecated
 */
template <>
class Deprecated<> final {
public:
    /**
     * @brief Check if a field is deprecated
     *
     * @param value The json value to check
     * @param key The key to check
     * @return A warning if the field is deprecated or std::nullopt otherwise
     */
    [[nodiscard]] static std::optional<Warning>
    check(boost::json::value const& value, std::string_view key)
    {
        if (value.is_object() and value.as_object().contains(key))
            return Warning{WarningCode::warnRPC_DEPRECATED, fmt::format("Field '{}' is deprecated.", key)};
        return std::nullopt;
    }
};

/**
 * @brief Check if a value of a field is deprecated
 * @tparam T The type of the field
 */
template <typename T>
class Deprecated<T> final {
    T value_;

public:
    /**
     * @brief Construct a new Deprecated object
     *
     * @param val The value that is deprecated
     */
    Deprecated(T val) : value_(val)
    {
    }

    /**
     * @brief Check if a value of a field is deprecated
     *
     * @param value The json value to check
     * @param key The key to check
     * @return A warning if the field is deprecated or std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Warning>
    check(boost::json::value const& value, std::string_view key) const
    {
        if (value.is_object() and value.as_object().contains(key) and
            validation::checkType<T>(value.as_object().at(key))) {
            using boost::json::value_to;
            auto const res = value_to<T>(value.as_object().at(key));
            if (value_ == res) {
                return Warning{
                    WarningCode::warnRPC_DEPRECATED, fmt::format("Value '{}' for field '{}' is deprecated", value_, key)
                };
            }
        }
        return {};
    }
};

/**
 * @brief Deduction guide for Deprecated
 */
template <typename... T>
Deprecated(T&&...) -> Deprecated<T...>;

}  // namespace rpc::check
