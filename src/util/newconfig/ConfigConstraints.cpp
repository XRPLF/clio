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

#include "util/newconfig/ConfigConstraints.hpp"

#include "util/newconfig/Errors.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <variant>

namespace util::config {

std::optional<Error>
PortConstraint::checkConstraint(Constraint::ValueType const& port) const
{
    uint32_t p = 0;
    if (!checkType(port))
        return Error{"Port must be an integer or string"};

    if (std::holds_alternative<std::string>(port)) {
        p = static_cast<uint32_t>(std::stoi(std::get<std::string>(port)));
    } else {
        p = static_cast<uint32_t>(std::get<int64_t>(port));
    }
    if (p >= portMin && p <= portMax)
        return std::nullopt;
    return Error{"Port does not satisfy the constraint bounds"};
}

std::optional<Error>
ChannelNameConstraint::checkConstraint(Constraint::ValueType const& channel) const
{
    if (!checkType(channel))
        return Error{"Channel name must be a string"};

    if (std::ranges::any_of(channels, [&channel](std::string_view name) {
            return std::get<std::string>(channel) == name;
        }))
        return std::nullopt;
    return Error{"Channel name must be one of General, WebServer, Backend, RPC, ETL, Subscriptions, Performance"};
}

std::optional<Error>
LogLevelNameConstraint::checkConstraint(Constraint::ValueType const& logger) const
{
    if (!checkType(logger))
        return Error{"log_level must be a string"};

    if (std::ranges::any_of(logLevels, [&logger](std::string_view name) {
            return std::get<std::string>(logger) == name;
        }))
        return std::nullopt;
    return Error{"log_level must be one of trace, debug, info, warning, error, fatal, count"};
}

std::optional<Error>
ValidIPConstraint::checkConstraint(Constraint::ValueType const& ip) const
{
    if (!checkType(ip))
        return Error{"ip must be a string"};

    static std::regex const ipv4Regex(
        R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
    );
    if (std::regex_match(std::get<std::string>(ip), ipv4Regex))
        return std::nullopt;
    return Error{"ip is not a valid ip address"};
}

std::optional<Error>
CassandraName::checkConstraint(ValueType const& name) const
{
    if (checkType(name) && std::get<std::string>(name) == "cassandra")
        return std::nullopt;
    return Error{"database.type must be string Cassandra"};
}

std::optional<Error>
LoadConstraint::checkConstraint(ValueType const& name) const
{
    if (!checkType(name))
        return Error{"cache.load must be a string"};

    auto const load = std::get<std::string>(name);
    if (load == "sync" || load == "async" || load == "none")
        return std::nullopt;
    return Error{"cache.load must be string sync, async, or none"};
}

std::optional<Error>
LogTagStyle::checkConstraint(ValueType const& tagName) const
{
    if (!checkType(tagName))
        return Error{"log_tag_style must be a string"};

    if (std::ranges::any_of(logTags, [&tagName](std::string_view name) {
            return std::get<std::string>(tagName) == name;
        }))
        return std::nullopt;
    return Error{"log_tag_style must be string int, uint, null, none, or uuid"};
}

std::optional<Error>
APIVersionConstraint::checkConstraint(ValueType const& apiVersion) const
{
    if (!checkType(apiVersion))
        return Error{"api_version must be an integer"};

    if (std::get<int64_t>(apiVersion) <= API_VERSION_MAX && std::get<int64_t>(apiVersion) >= API_VERSION_MIN)
        return std::nullopt;
    return Error{fmt::format("api_version must be between {} and {}", API_VERSION_MIN, API_VERSION_MAX)};
}

}  // namespace util::config
