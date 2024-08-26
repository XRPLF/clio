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

#include "rpc/common/APIVersion.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/Errors.hpp"
#include "util/newconfig/Types.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace util::config {

std::optional<Error>
PortConstraint::checkTypeImpl(Value const& port) const
{
    if (!(std::holds_alternative<int64_t>(port) || std::holds_alternative<std::string>(port)))
        return Error{"Port must be a string or Integer"};
    return std::nullopt;
}

std::optional<Error>
PortConstraint::checkValueImpl(Value const& port) const
{
    uint32_t p = 0;
    if (std::holds_alternative<std::string>(port)) {
        try {
            p = static_cast<uint32_t>(std::stoi(std::get<std::string>(port)));
        } catch (std::invalid_argument const& e) {
            return Error{"Port string must be an integer."};
        }
    } else {
        p = static_cast<uint32_t>(std::get<int64_t>(port));
    }
    if (p >= portMin && p <= portMax)
        return std::nullopt;
    return Error{"Port does not satisfy the constraint bounds"};
}

std::optional<Error>
ChannelNameConstraint::checkTypeImpl(Value const& channelName) const
{
    if (!std::holds_alternative<std::string>(channelName))
        return Error{"Key \"channel\"'s value must be a string"};
    return std::nullopt;
}

std::optional<Error>
ChannelNameConstraint::checkValueImpl(Value const& channelName) const
{
    if (std::ranges::any_of(Logger::CHANNELS, [&channelName](std::string_view name) {
            return std::get<std::string>(channelName) == name;
        }))
        return std::nullopt;
    return Error{makeErrorMsg("channel", Logger::CHANNELS)};
}

std::optional<Error>
LogLevelNameConstraint::checkTypeImpl(Value const& logLevel) const
{
    if (!std::holds_alternative<std::string>(logLevel))
        return Error{"Key \"log_level\"'s value must be a string"};
    return std::nullopt;
}

std::optional<Error>
LogLevelNameConstraint::checkValueImpl(Value const& logLevel) const
{
    if (std::ranges::any_of(logLevels, [&logLevel](std::string_view name) {
            return std::get<std::string>(logLevel) == name;
        }))
        return std::nullopt;
    return Error{makeErrorMsg("log_level", logLevels)};
}

std::optional<Error>
ValidIPConstraint::checkTypeImpl(Value const& ip) const
{
    if (!std::holds_alternative<std::string>(ip))
        return Error{"ip value must be a string"};
    return std::nullopt;
}

std::optional<Error>
ValidIPConstraint::checkValueImpl(Value const& ip) const
{
    static std::regex const ipv4(
        R"(^((http|https):\/\/)?((([a-zA-Z0-9-]+\.)+[a-zA-Z]{2,6})|(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))(:\d{1,5})?(\/[^\s]*)?$)"
    );
    if (std::regex_match(std::get<std::string>(ip), ipv4))
        return std::nullopt;

    return Error{"ip is not a valid ip address"};
}

std::optional<Error>
CassandraName::checkTypeImpl(Value const& name) const
{
    if (!std::holds_alternative<std::string>(name))
        return Error{"Key \"database.type\"'s value must be a string"};
    return std::nullopt;
}

std::optional<Error>
CassandraName::checkValueImpl(Value const& name) const
{
    if (std::get<std::string>(name) == "cassandra")
        return std::nullopt;
    return Error{"Key \"database.type\"'s value must be string Cassandra"};
}

std::optional<Error>
LoadConstraint::checkTypeImpl(Value const& loadMode) const
{
    if (!std::holds_alternative<std::string>(loadMode))
        return Error{"Key \"cache.load\" value must be a string"};
    return std::nullopt;
}

std::optional<Error>
LoadConstraint::checkValueImpl(Value const& loadMode) const
{
    if (std::ranges::any_of(loadCacheMode, [&loadMode](std::string_view name) {
            return std::get<std::string>(loadMode) == name;
        }))
        return std::nullopt;
    return Error{makeErrorMsg("cache.load", loadCacheMode)};
}

std::optional<Error>
LogTagStyle::checkTypeImpl(Value const& tagName) const
{
    if (!std::holds_alternative<std::string>(tagName))
        return Error{"Key \"log_tag_style\"'s value must be a string"};
    return std::nullopt;
}

std::optional<Error>
LogTagStyle::checkValueImpl(Value const& tagName) const
{
    if (std::ranges::any_of(logTags, [&tagName](std::string_view name) {
            return std::get<std::string>(tagName) == name;
        })) {
        return std::nullopt;
    }
    return Error{makeErrorMsg("log_tag_style", logTags)};
}

std::optional<Error>
APIVersionConstraint::checkTypeImpl(Value const& apiVersion) const
{
    if (!std::holds_alternative<int64_t>(apiVersion))
        return Error{"api_version value must be a positive integer"};
    return std::nullopt;
}

std::optional<Error>
APIVersionConstraint::checkValueImpl(Value const& apiVersion) const
{
    if (std::get<int64_t>(apiVersion) <= rpc::API_VERSION_MAX && std::get<int64_t>(apiVersion) >= rpc::API_VERSION_MIN)
        return std::nullopt;
    return Error{fmt::format("api_version must be between {} and {}", rpc::API_VERSION_MIN, rpc::API_VERSION_MAX)};
}

std::optional<Error>
PositiveDouble::checkTypeImpl(Value const& type) const
{
    if (!std::holds_alternative<double>(type))
        return Error{"double number must be of type double"};
    return std::nullopt;
}

std::optional<Error>
PositiveDouble::checkValueImpl(Value const& num) const
{
    if (std::get<double>(num) >= 0)
        return std::nullopt;
    return Error{"double number must be greater than 0"};
}

}  // namespace util::config
