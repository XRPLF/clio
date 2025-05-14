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

#include "util/config/ConfigConstraints.hpp"

#include "rpc/RPCCenter.hpp"
#include "util/config/Error.hpp"
#include "util/config/Types.hpp"

#include <boost/asio/ip/address.hpp>

#include <cstdint>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <variant>

namespace util::config {

std::optional<Error>
PortConstraint::checkTypeImpl(Value const& port) const
{
    if (!(std::holds_alternative<int64_t>(port) || std::holds_alternative<std::string>(port)))
        return Error{"Port must be a string or integer"};
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
    if (p >= kPORT_MIN && p <= kPORT_MAX)
        return std::nullopt;
    return Error{"Port does not satisfy the constraint bounds"};
}

std::optional<Error>
ValidIPConstraint::checkTypeImpl(Value const& ip) const
{
    if (!std::holds_alternative<std::string>(ip))
        return Error{"Ip value must be a string"};
    return std::nullopt;
}

std::optional<Error>
ValidIPConstraint::checkValueImpl(Value const& ip) const
{
    boost::system::error_code errorCode;
    boost::asio::ip::make_address(std::get<std::string>(ip), errorCode);
    if (not errorCode.failed())
        return std::nullopt;

    static std::regex const kHOST{
        R"regex(^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\-]*[a-zA-Z0-9])\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\-]*[A-Za-z0-9])$)regex"
    };

    if (std::regex_match(std::get<std::string>(ip), kHOST))
        return std::nullopt;

    return Error{"Ip is not a valid ip address or hostname"};
}

std::optional<Error>
PositiveDouble::checkTypeImpl(Value const& num) const
{
    if (!(std::holds_alternative<double>(num) || std::holds_alternative<int64_t>(num)))
        return Error{"Double number must be of type int or double"};
    return std::nullopt;
}

std::optional<Error>
PositiveDouble::checkValueImpl(Value const& num) const
{
    if (std::holds_alternative<int64_t>(num) && std::get<int64_t>(num) >= 0)
        return std::nullopt;
    if (std::get<double>(num) >= 0)
        return std::nullopt;
    return Error{"Double number must be greater than or equal to 0"};
}

std::optional<Error>
RpcNameConstraint::checkTypeImpl(Value const& value) const
{
    if (not std::holds_alternative<std::string>(value))
        return Error{"RPC command name must be a string"};
    return std::nullopt;
}

std::optional<Error>
RpcNameConstraint::checkValueImpl(Value const& value) const
{
    auto const str = std::get<std::string>(value);
    if (not rpc::RPCCenter::isRpcName(str))
        return Error{"Invalid RPC command name"};

    return std::nullopt;
}

}  // namespace util::config
