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

#include "util/newconfig/ConfigDefinition.hpp"

#include "util/Assert.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <fmt/core.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <variant>

namespace util::config {
/**
 * @brief Full Clio Configuration definition.
 *
 * Specifies which keys are valid in Clio Config and provides default values if user's do not specify one. Those
 * without default values must be present in the user's config file.
 */
static ClioConfigDefinition ClioConfig = ClioConfigDefinition{
    {{"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra")},
     {"database.cassandra.contact_points", ConfigValue{ConfigType::String}.defaultValue("localhost")},
     {"database.cassandra.port", ConfigValue{ConfigType::Integer}},
     {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue("clio")},
     {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(3u)},
     {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.defaultValue("table_prefix")},
     {"database.cassandra.max_write_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(10'000)},
     {"database.cassandra.max_read_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(100'000)},
     {"database.cassandra.threads",
      ConfigValue{ConfigType::Integer}.defaultValue(static_cast<uint32_t>(std::thread::hardware_concurrency()))},
     {"database.cassandra.core_connections_per_host", ConfigValue{ConfigType::Integer}.defaultValue(1)},
     {"database.cassandra.queue_size_io", ConfigValue{ConfigType::Integer}.optional()},
     {"database.cassandra.write_batch_size", ConfigValue{ConfigType::Integer}.defaultValue(20)},
     {"etl_source.[].ip", Array{ConfigValue{ConfigType::String}.optional()}},
     {"etl_source.[].ws_port", Array{ConfigValue{ConfigType::String}.optional().min(1).max(65535)}},
     {"etl_source.[].grpc_port", Array{ConfigValue{ConfigType::String}.optional().min(1).max(65535)}},
     {"forwarding_cache_timeout", ConfigValue{ConfigType::Integer}},
     {"dos_guard.[].whitelist", Array{ConfigValue{ConfigType::String}}},
     {"dos_guard.max_fetches", ConfigValue{ConfigType::Integer}.defaultValue(1000'000)},
     {"dos_guard.max_connections", ConfigValue{ConfigType::Integer}.defaultValue(20)},
     {"dos_guard.max_requests", ConfigValue{ConfigType::Integer}.defaultValue(20)},
     {"dos_guard.sweep_interval", ConfigValue{ConfigType::Double}.defaultValue(1.0)},
     {"cache.peers.[].ip", Array{ConfigValue{ConfigType::String}}},
     {"cache.peers.[].port", Array{ConfigValue{ConfigType::String}}},
     {"server.ip", ConfigValue{ConfigType::String}},
     {"server.port", ConfigValue{ConfigType::Integer}},
     {"server.max_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(0)},
     {"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
     {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
     {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
     {"io_threads", ConfigValue{ConfigType::Integer}.defaultValue(2)},
     {"cache.num_diffs", ConfigValue{ConfigType::Integer}.defaultValue(32)},
     {"cache.num_markers", ConfigValue{ConfigType::Integer}.defaultValue(48)},
     {"cache.page_fetch_size", ConfigValue{ConfigType::Integer}.defaultValue(512)},
     {"cache.load", ConfigValue{ConfigType::String}.defaultValue("async")},
     {"log_channels.[].channel", Array{ConfigValue{ConfigType::String}.optional()}},
     {"log_channels.[].log_level", Array{ConfigValue{ConfigType::String}.optional()}},
     {"log_level", ConfigValue{ConfigType::String}.defaultValue("info")},
     {"log_format",
      ConfigValue{ConfigType::String}.defaultValue(
          R"(%TimeStamp% (%SourceLocation%) [%ThreadID%] %Channel%:%Severity% %Message%)"
      )},
     {"log_to_console", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
     {"log_directory", ConfigValue{ConfigType::String}.optional()},
     {"log_rotation_size", ConfigValue{ConfigType::Integer}.defaultValue(2048)},
     {"log_directory_max_size", ConfigValue{ConfigType::Integer}.defaultValue(50 * 1024)},
     {"log_rotation_hour_interval", ConfigValue{ConfigType::Integer}.defaultValue(12)},
     {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")},
     {"extractor_threads", ConfigValue{ConfigType::Integer}.defaultValue(2u)},
     {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
     {"txn_threshold", ConfigValue{ConfigType::Integer}.defaultValue(0)},
     {"start_sequence", ConfigValue{ConfigType::String}.optional()},
     {"finish_sequence", ConfigValue{ConfigType::String}.optional()},
     {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()},
     {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},
     {"api_version.min", ConfigValue{ConfigType::Integer}},
     {"api_version.max", ConfigValue{ConfigType::Integer}}}
};

ObjectView
ClioConfigDefinition::getObject(std::string_view prefix) const
{
    auto const prefixWithDot = std::string(prefix) + ".";
    for (auto const& [mapKey, mapVal] : map_) {
        if (mapKey.starts_with(prefixWithDot))
            ASSERT(!mapKey.ends_with(".[]"), "Trying to retrieve an object when value is an Array");

        if (mapKey.starts_with(prefixWithDot) && std::holds_alternative<ConfigValue>(mapVal)) {
            ASSERT(std::holds_alternative<ConfigValue>(mapVal), "Trying to get object from Array but requires index");
            return ObjectView{prefix, *this};
        }
    }
    throw std::invalid_argument(fmt::format("Key {} is not found in config", prefixWithDot));
}

ObjectView
ClioConfigDefinition::getObject(std::string_view prefix, std::size_t idx) const
{
    auto const prefixWithDot = std::string(prefix) + ".";
    for (auto const& [mapKey, mapVal] : map_) {
        if (mapKey.starts_with(prefixWithDot))
            ASSERT(!mapKey.ends_with(".[]"), "Trying to retrieve an object when value is an Array");

        if (mapKey.starts_with(prefixWithDot)) {
            ASSERT(std::holds_alternative<Array>(mapVal), "Trying to get object, but doesn't require index");
            ASSERT(std::get<Array>(mapVal).size() > idx, "Index provided is out of scope");
            return ObjectView{prefix, idx, *this};
        }
    }
    throw std::invalid_argument(fmt::format("Key {} is not found in config", prefixWithDot));
}

ArrayView
ClioConfigDefinition::getArray(std::string_view prefix) const
{
    auto key = std::string(prefix);
    if (!prefix.contains(".[]"))
        key += ".[]";

    for (auto const& [mapKey, mapVal] : map_) {
        if (mapKey.starts_with(key)) {
            ASSERT(std::holds_alternative<Array>(mapVal), "Trying to retrieve an Array when value is an object ");
            return ArrayView{key, *this};
        }
    }
    throw std::invalid_argument(fmt::format("Key {} is not found in config", key));
}

ValueView
ClioConfigDefinition::getValue(std::string_view fullKey) const
{
    if (map_.contains(fullKey) && std::holds_alternative<ConfigValue>(map_.at(fullKey))) {
        return ValueView{std::get<ConfigValue>(map_.at(fullKey))};
    }
    ASSERT(
        map_.contains(fullKey) && std::holds_alternative<Array>(map_.at(fullKey)),
        "Value of Key {} is not Config Value.",
        fullKey
    );

    throw std::invalid_argument(fmt::format("Key {} is not found in config", fullKey));
}

Array const&
ClioConfigDefinition::atArray(std::string_view key) const
{
    ASSERT(map_.contains(key), "Current string {} is a prefix, not a key of config", key);
    ASSERT(std::holds_alternative<Array>(map_.at(key)), "Value of {} is not an array", key);
    return std::get<Array>(map_.at(key));
}

std::size_t
ClioConfigDefinition::arraySize(std::string_view prefix) const
{
    ASSERT(prefix.contains(".[]"), "Prefix {} is not an array", prefix);
    for (auto const& pair : map_) {
        if (pair.first.starts_with(prefix)) {
            return std::get<Array>(pair.second).size();
        }
    }
    throw std::logic_error(fmt::format("Prefix {} not found in any of the config keys", prefix));
}

}  // namespace util::config
