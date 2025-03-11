//------------------------------------------------------------------------------
/*
   This file is part of clio: https://github.com/XRPLF/clio
   Copyright (c) 2025, the clio developers.

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

#include "util/newconfig/Types.hpp"

#include <cstdint>
#include <ostream>
#include <string>
#include <variant>

namespace util::config {

std::ostream&
operator<<(std::ostream& stream, ConfigType type)
{
    switch (type) {
        case ConfigType::Integer:
            stream << "int";
            break;
        case ConfigType::String:
            stream << "string";
            break;
        case ConfigType::Double:
            stream << "double";
            break;
        case ConfigType::Boolean:
            stream << "boolean";
            break;
        default:
            stream << "unsupported type";
    }
    return stream;
}

std::ostream&
operator<<(std::ostream& stream, Value value)
{
    if (std::holds_alternative<std::string>(value)) {
        stream << std::get<std::string>(value);
    } else if (std::holds_alternative<bool>(value)) {
        stream << (std::get<bool>(value) ? "False" : "True");
    } else if (std::holds_alternative<double>(value)) {
        stream << std::get<double>(value);
    } else if (std::holds_alternative<int64_t>(value)) {
        stream << std::get<int64_t>(value);
    }
    return stream;
}

}  // namespace util::config
