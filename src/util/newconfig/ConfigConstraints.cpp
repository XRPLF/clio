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

#include "util/newconfig/Error.hpp"
#include "util/newconfig/Types.hpp"

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
    if (p >= portMin && p <= portMax)
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
    if (std::get<std::string>(ip) == "localhost")
        return std::nullopt;

    static std::regex const ipv4(
        R"(^((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])$)"
    );

    static std::regex const ip_url(
        R"(^((http|https):\/\/)?((([a-zA-Z0-9-]+\.)+[a-zA-Z]{2,6})|(((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])))(:\d{1,5})?(\/[^\s]*)?$)"
    );
    if (std::regex_match(std::get<std::string>(ip), ipv4) || std::regex_match(std::get<std::string>(ip), ip_url))
        return std::nullopt;

    return Error{"Ip is not a valid ip address"};
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
    if (std::get<double>(num) >= 0)
        return std::nullopt;
    return Error{"Double number must be greater than 0"};
}

}  // namespace util::config
