//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <boost/json/conversion.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>

#include <stdexcept>

namespace rpc {

/**
 * @brief A wrapper around bool that allows to convert from any JSON value
 */
struct JsonBool {
    bool value = false;

    operator bool() const
    {
        return value;
    }
};

inline JsonBool
tag_invoke(boost::json::value_to_tag<JsonBool> const&, boost::json::value const& jsonValue)
{
    switch (jsonValue.kind()) {
        case boost::json::kind::null:
            return JsonBool{false};
        case boost::json::kind::bool_:
            return JsonBool{jsonValue.as_bool()};
        case boost::json::kind::uint64:
            [[fallthrough]];
        case boost::json::kind::int64:
            return JsonBool{jsonValue.as_int64() != 0};
        case boost::json::kind::double_:
            return JsonBool{jsonValue.as_double() != 0.0};
        case boost::json::kind::string:
            // Also should be `jsonValue.as_string() != "false"` but rippled doesn't do
            // that. Anyway for v2 api we have bool validation
            return JsonBool{!jsonValue.as_string().empty() && jsonValue.as_string()[0] != 0};
        case boost::json::kind::array:
            return JsonBool{!jsonValue.as_array().empty()};
        case boost::json::kind::object:
            return JsonBool{!jsonValue.as_object().empty()};
    }
    throw std::runtime_error("Invalid json value");
}

}  // namespace rpc
