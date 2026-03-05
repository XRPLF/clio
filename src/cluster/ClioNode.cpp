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

#include "etl/WriterState.hpp"
#include "util/TimeUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/uuid/uuid.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace cluster {

namespace {

struct JsonFields {
    static constexpr std::string_view const kUPDATE_TIME = "update_time";
    static constexpr std::string_view const kDB_ROLE = "db_role";
    static constexpr std::string_view const kETL_STARTED = "etl_started";
    static constexpr std::string_view const kCACHE_IS_FULL = "cache_is_full";
};

}  // namespace

ClioNode
ClioNode::from(ClioNode::Uuid uuid, etl::WriterStateInterface const& writerState)
{
    auto const dbRole = [&writerState]() {
        if (writerState.isReadOnly()) {
            return ClioNode::DbRole::ReadOnly;
        }
        if (writerState.isFallback()) {
            return ClioNode::DbRole::Fallback;
        }

        return writerState.isWriting() ? ClioNode::DbRole::Writer : ClioNode::DbRole::NotWriter;
    }();
    return ClioNode{
        .uuid = std::move(uuid),
        .updateTime = std::chrono::system_clock::now(),
        .dbRole = dbRole,
        .etlStarted = writerState.isEtlStarted(),
        .cacheIsFull = writerState.isCacheFull()
    };
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, ClioNode const& node)
{
    jv = {
        {JsonFields::kUPDATE_TIME, util::systemTpToUtcStr(node.updateTime, ClioNode::kTIME_FORMAT)},
        {JsonFields::kDB_ROLE, static_cast<int64_t>(node.dbRole)},
        {JsonFields::kETL_STARTED, node.etlStarted},
        {JsonFields::kCACHE_IS_FULL, node.cacheIsFull}
    };
}

ClioNode
tag_invoke(boost::json::value_to_tag<ClioNode>, boost::json::value const& jv)
{
    auto const& updateTimeStr = jv.as_object().at(JsonFields::kUPDATE_TIME).as_string();
    auto const updateTime =
        util::systemTpFromUtcStr(std::string(updateTimeStr), ClioNode::kTIME_FORMAT);
    if (!updateTime.has_value()) {
        throw std::runtime_error("Failed to parse update time");
    }

    auto const dbRoleValue = jv.as_object().at(JsonFields::kDB_ROLE).as_int64();
    if (dbRoleValue > static_cast<int64_t>(ClioNode::DbRole::MAX))
        throw std::runtime_error("Invalid db_role value");

    auto const etlStarted = jv.as_object().at(JsonFields::kETL_STARTED).as_bool();
    auto const cacheIsFull = jv.as_object().at(JsonFields::kCACHE_IS_FULL).as_bool();

    return ClioNode{
        // Json data doesn't contain uuid so leaving it empty here. It will be filled outside of
        // this parsing
        .uuid = std::make_shared<boost::uuids::uuid>(),
        .updateTime = updateTime.value(),
        .dbRole = static_cast<ClioNode::DbRole>(dbRoleValue),
        .etlStarted = etlStarted,
        .cacheIsFull = cacheIsFull
    };
}

}  // namespace cluster
