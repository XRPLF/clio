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
#include <fmt/core.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rpc::check {

struct Warning {
    Warning(WarningCode code, std::string message) : warningCode(code), extraMessage(std::move(message))
    {
    }

    WarningCode warningCode;
    std::string extraMessage;
};
using Warnings = std::vector<Warning>;

template <typename... T>
class Deprecated;

template <>
class Deprecated<> final {
public:
    [[nodiscard]] static std::optional<Warning>
    check(boost::json::value const& value, std::string_view key)
    {
        if (value.is_object() and value.as_object().contains(key))
            return Warning{WarningCode::warnRPC_DEPRECATED, fmt::format("Field '{}' is deprecated.", key)};
        return std::nullopt;
    }
};

template <typename T>
class Deprecated<T> final {
    T value_;

public:
    Deprecated(T val) : value_(val)
    {
    }

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

template <typename... T>
Deprecated(T&&...) -> Deprecated<T...>;

}  // namespace rpc::check
