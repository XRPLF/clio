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

#include "cluster/ClioNode.hpp"

#include "util/TimeUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cluster {

namespace {

struct Fields {
    static constexpr std::string_view const kUPDATE_TIME = "update_time";
};

}  // namespace

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, ClioNode const& node)
{
    jv = {
        {Fields::kUPDATE_TIME, util::systemTpToUtcStr(node.updateTime, ClioNode::kTIME_FORMAT)},
    };
}

ClioNode
tag_invoke(boost::json::value_to_tag<ClioNode>, boost::json::value const& jv)
{
    auto const& updateTimeStr = jv.as_object().at(Fields::kUPDATE_TIME).as_string();
    auto const updateTime = util::systemTpFromUtcStr(std::string(updateTimeStr), ClioNode::kTIME_FORMAT);
    if (!updateTime.has_value()) {
        throw std::runtime_error("Failed to parse update time");
    }

    return ClioNode{.uuid = std::make_shared<boost::uuids::uuid>(), .updateTime = updateTime.value()};
}

}  // namespace cluster
