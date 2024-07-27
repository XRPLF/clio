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
#include "util/newconfig/ValueView.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
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
     {"forwarding.cache_timeout", ConfigValue{ConfigType::Double}.defaultValue(0.0)},
     {"forwarding.request_timeout", ConfigValue{ConfigType::Double}.defaultValue(10.0)},
     {"dos_guard.whitelist.[]", Array{ConfigValue{ConfigType::String}}},
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
     {"cache.num_cursors_from_diff", ConfigValue{ConfigType::Integer}.defaultValue(0)},
     {"cache.num_cursors_from_account", ConfigValue{ConfigType::Integer}.defaultValue(0)},
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

ClioConfigDefinition::ClioConfigDefinition(std::initializer_list<KeyValuePair> pair)
{
    for (auto const& p : pair) {
        if (p.first.contains("[]"))
            ASSERT(std::holds_alternative<Array>(p.second), "Value must be array if key has \"[]\"");
        map_.insert(p);
    }
}

ObjectView
ClioConfigDefinition::getObject(std::string_view prefix, std::optional<std::size_t> idx) const
{
    auto const prefixWithDot = std::string(prefix) + ".";
    for (auto const& [mapKey, mapVal] : map_) {
        auto const hasPrefix = mapKey.starts_with(prefixWithDot);
        if (idx.has_value() && hasPrefix && std::holds_alternative<Array>(mapVal)) {
            ASSERT(std::get<Array>(mapVal).size() > idx.value(), "Index provided is out of scope");
            // we want to support getObject("array") and getObject("array.[]"), so we check if "[]" exists
            if (!prefix.contains("[]"))
                return ObjectView{prefixWithDot + "[]", idx.value(), *this};
            return ObjectView{prefix, idx.value(), *this};
        }
        if (hasPrefix && !idx.has_value() && !mapKey.contains(prefixWithDot + "[]")) {
            ASSERT(!mapKey.contains(prefixWithDot + "[]"), "Key {} is an array, not an object", mapKey);
            return ObjectView{prefix, *this};
        }
    }
    ASSERT(false, "Key {} is not found in config", prefixWithDot);
    return ObjectView{"", *this};
}

ArrayView
ClioConfigDefinition::getArray(std::string_view prefix) const
{
    auto key = checkForBracketsInArray(prefix);

    for (auto const& [mapKey, mapVal] : map_) {
        if (mapKey.starts_with(key)) {
            ASSERT(std::holds_alternative<Array>(mapVal), "Trying to retrieve an Array when value is an object ");
            return ArrayView{key, *this};
        }
    }
    ASSERT(false, "Key {} is not found in config", key);
    return ArrayView{"", *this};
}

bool
ClioConfigDefinition::contains(std::string_view key) const
{
    return map_.contains(key);
}

bool
ClioConfigDefinition::startsWith(std::string_view key) const
{
    auto it = std::find_if(map_.begin(), map_.end(), [&key](auto const& pair) { return pair.first.starts_with(key); });
    return it != map_.end();
}

ValueView
ClioConfigDefinition::getValue(std::string_view fullKey) const
{
    ASSERT(map_.contains(fullKey), "key {} does not exist in config", fullKey);
    if (std::holds_alternative<ConfigValue>(map_.at(fullKey))) {
        return ValueView{std::get<ConfigValue>(map_.at(fullKey))};
    }
    ASSERT(false, "Value of key {} is an Array, not an object", fullKey);
    return ValueView{ConfigValue{}};
}

ValueView
ClioConfigDefinition::getValueInArray(std::string_view fullKey, std::size_t index) const
{
    auto it = this->getArrayIterator(fullKey);
    return ValueView{std::get<Array>(it->second).at(index)};
}

Array const&
ClioConfigDefinition::atArray(std::string_view fullKey) const
{
    auto it = this->getArrayIterator(fullKey);
    return std::get<Array>(it->second);
}

std::size_t
ClioConfigDefinition::arraySize(std::string_view prefix) const
{
    auto key = checkForBracketsInArray(prefix);

    for (auto const& pair : map_) {
        if (pair.first.starts_with(key)) {
            return std::get<Array>(pair.second).size();
        }
    }
    ASSERT(false, "Prefix {} not found in any of the config keys", key);
    return 0;
}

}  // namespace util::config
