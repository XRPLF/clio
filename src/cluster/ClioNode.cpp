#include "cluster/ClioNode.hpp"

#include "data/LedgerCacheLoadingState.hpp"
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
    static constexpr std::string_view const kCACHE_IS_CURRENTLY_LOADING =
        "cache_is_currently_loading";
};

}  // namespace

ClioNode
ClioNode::from(
    ClioNode::Uuid uuid,
    etl::WriterStateInterface const& writerState,
    data::LedgerCacheLoadingStateInterface const& cacheLoadingState
)
{
    auto const dbRole = [&writerState]() {
        if (writerState.isReadOnly()) {
            return ClioNode::DbRole::ReadOnly;
        }
        if (writerState.isFallback()) {
            return ClioNode::DbRole::Fallback;
        }

        if (writerState.isFallbackRecovery()) {
            return ClioNode::DbRole::FallbackRecovery;
        }

        return writerState.isWriting() ? ClioNode::DbRole::Writer : ClioNode::DbRole::NotWriter;
    }();
    return ClioNode{
        .uuid = std::move(uuid),
        .updateTime = std::chrono::system_clock::now(),
        .dbRole = dbRole,
        .etlStarted = writerState.isEtlStarted(),
        .cacheIsFull = writerState.isCacheFull(),
        .cacheIsCurrentlyLoading = cacheLoadingState.isCurrentlyLoading()
    };
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, ClioNode const& node)
{
    jv = {
        {JsonFields::kUPDATE_TIME, util::systemTpToUtcStr(node.updateTime, ClioNode::kTIME_FORMAT)},
        {JsonFields::kDB_ROLE, static_cast<int64_t>(node.dbRole)},
        {JsonFields::kETL_STARTED, node.etlStarted},
        {JsonFields::kCACHE_IS_FULL, node.cacheIsFull},
        {JsonFields::kCACHE_IS_CURRENTLY_LOADING, node.cacheIsCurrentlyLoading}
    };
}

ClioNode
tag_invoke(boost::json::value_to_tag<ClioNode>, boost::json::value const& jv)
{
    auto const& obj = jv.as_object();
    auto const& updateTimeStr = obj.at(JsonFields::kUPDATE_TIME).as_string();
    auto const updateTime =
        util::systemTpFromUtcStr(std::string(updateTimeStr), ClioNode::kTIME_FORMAT);
    if (!updateTime.has_value()) {
        throw std::runtime_error("Failed to parse update time");
    }

    // Each field has a default value for backward compatibility
    auto dbRole = ClioNode::DbRole::Fallback;
    if (auto const* v = obj.if_contains(JsonFields::kDB_ROLE)) {
        auto const dbRoleValue = v->as_int64();
        if (dbRoleValue > static_cast<int64_t>(ClioNode::DbRole::Max))
            throw std::runtime_error("Invalid db_role value");
        dbRole = static_cast<ClioNode::DbRole>(dbRoleValue);
    }

    auto const etlStarted =
        obj.contains(JsonFields::kETL_STARTED) ? obj.at(JsonFields::kETL_STARTED).as_bool() : true;
    auto const cacheIsFull = obj.contains(JsonFields::kCACHE_IS_FULL)
        ? obj.at(JsonFields::kCACHE_IS_FULL).as_bool()
        : true;
    auto const cacheIsCurrentlyLoading = obj.contains(JsonFields::kCACHE_IS_CURRENTLY_LOADING)
        ? obj.at(JsonFields::kCACHE_IS_CURRENTLY_LOADING).as_bool()
        : false;

    return ClioNode{
        // Json data doesn't contain uuid so leaving it empty here. It will be filled outside of
        // this parsing
        .uuid = std::make_shared<boost::uuids::uuid>(),
        .updateTime = *updateTime,
        .dbRole = dbRole,
        .etlStarted = etlStarted,
        .cacheIsFull = cacheIsFull,
        .cacheIsCurrentlyLoading = cacheIsCurrentlyLoading
    };
}

}  // namespace cluster
