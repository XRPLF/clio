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

#include "etl/ETLState.hpp"

#include "rpc/JS.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <optional>

namespace etl {

std::optional<ETLState>
tag_invoke(boost::json::value_to_tag<std::optional<ETLState>>, boost::json::value const& jv)
{
    ETLState state;
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(error)))
        return std::nullopt;

    if (jsonObject.contains(JS(result)) && jsonObject.at(JS(result)).as_object().contains(JS(info))) {
        auto const rippledInfo = jsonObject.at(JS(result)).as_object().at(JS(info)).as_object();
        if (rippledInfo.contains(JS(network_id)))
            state.networkID.emplace(boost::json::value_to<int64_t>(rippledInfo.at(JS(network_id))));
    }

    return state;
}

}  // namespace etl
