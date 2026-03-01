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

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

#include <concepts>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

namespace rpc::validation {
namespace impl {

template <std::unsigned_integral Expected>
void
clampAs(boost::json::value& value)
{
    if (value.is_uint64()) {
        auto const valueUint = value.as_uint64();
        if (valueUint > static_cast<uint64_t>(std::numeric_limits<Expected>::max()))
            value = std::numeric_limits<Expected>::max();
    } else if (value.is_int64()) {
        auto const valueInt = value.as_int64();
        if (valueInt > static_cast<int64_t>(std::numeric_limits<Expected>::max()))
            value = std::numeric_limits<Expected>::max();
    }
}

template <std::signed_integral Expected>
void
clampAs(boost::json::value& value)
{
    if (value.is_uint64()) {
        auto const valueUint = value.as_uint64();
        if (valueUint > static_cast<uint64_t>(std::numeric_limits<Expected>::max()))
            value = std::numeric_limits<Expected>::max();
    } else if (value.is_int64()) {
        auto const valueInt = value.as_int64();
        if (valueInt > static_cast<int64_t>(std::numeric_limits<Expected>::max())) {
            value = std::numeric_limits<Expected>::max();
        } else if (valueInt < static_cast<int64_t>(std::numeric_limits<Expected>::min())) {
            value = std::numeric_limits<Expected>::min();
        }
    }
}

}  // namespace impl

/**
 * @brief Check that the type is the same as what was expected.
 *
 * @tparam Expected The expected type that value should be convertible to
 * @param value The json value to check the type of
 * @return true if convertible; false otherwise
 */
template <typename Expected>
[[nodiscard]] bool
checkType(boost::json::value const& value)
{
    auto hasError = false;
    if constexpr (std::is_same_v<Expected, bool>) {
        if (not value.is_bool())
            hasError = true;
    } else if constexpr (std::is_same_v<Expected, std::string>) {
        if (not value.is_string())
            hasError = true;
    } else if constexpr (std::is_same_v<Expected, double> or std::is_same_v<Expected, float>) {
        if (not value.is_double())
            hasError = true;
    } else if constexpr (std::is_same_v<Expected, boost::json::array>) {
        if (not value.is_array())
            hasError = true;
    } else if constexpr (std::is_same_v<Expected, boost::json::object>) {
        if (not value.is_object())
            hasError = true;
    } else if constexpr (
        std::is_convertible_v<Expected, uint64_t> or std::is_convertible_v<Expected, int64_t>
    ) {
        if (not value.is_int64() && not value.is_uint64())
            hasError = true;
        // if the type specified is unsigned, it should not be negative
        if constexpr (std::is_unsigned_v<Expected>) {
            if (value.is_int64() and value.as_int64() < 0)
                hasError = true;
        }
    }

    return not hasError;
}

/**
 * @brief Check that the type is the same as what was expected optionally clamping it into range.
 *
 * This is used to automatically clamp the value into the range available to the specified type. It
 * is needed in order to avoid Min, Max and other validators throw "not exact" error from Boost.Json
 * library if the value does not fit in the specified type.
 *
 * @tparam Expected The expected type that value should be convertible to
 * @param value The json value to check the type of
 * @return true if convertible; false otherwise
 */
template <typename Expected>
[[nodiscard]] bool
checkTypeAndClamp(boost::json::value& value)
{
    if (not checkType<Expected>(value))
        return false;  // fails basic type check

    if constexpr (std::is_integral_v<Expected> and not std::is_same_v<Expected, bool>)
        impl::clampAs<Expected>(value);

    return true;
}

}  // namespace rpc::validation
