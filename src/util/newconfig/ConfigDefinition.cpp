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
#include "util/OverloadSet.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigFileInterface.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Error.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/Types.hpp"
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
#include <utility>
#include <variant>
#include <vector>

namespace util::config {
/**
 * @brief Full Clio Configuration definition.
 *
 * Specifies which keys are valid in Clio Config and provides default values if user's do not specify one. Those
 * without default values must be present in the user's config file.
 */
static ClioConfigDefinition ClioConfig = ClioConfigDefinition{
    {{"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra").withConstraint(validateCassandraName)},
     {"database.cassandra.contact_points", ConfigValue{ConfigType::String}.defaultValue("localhost")},
     {"database.cassandra.port", ConfigValue{ConfigType::Integer}.withConstraint(validatePort)},
     {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue("clio")},
     {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(3u)},
     {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.defaultValue("table_prefix")},
     {"database.cassandra.max_write_requests_outstanding",
      ConfigValue{ConfigType::Integer}.defaultValue(10'000).withConstraint(validateUint32)},
     {"database.cassandra.max_read_requests_outstanding",
      ConfigValue{ConfigType::Integer}.defaultValue(100'000).withConstraint(validateUint32)},
     {"database.cassandra.threads",
      ConfigValue{ConfigType::Integer}
          .defaultValue(static_cast<uint32_t>(std::thread::hardware_concurrency()))
          .withConstraint(validateUint32)},
     {"database.cassandra.core_connections_per_host",
      ConfigValue{ConfigType::Integer}.defaultValue(1).withConstraint(validateUint16)},
     {"database.cassandra.queue_size_io", ConfigValue{ConfigType::Integer}.optional().withConstraint(validateUint16)},
     {"database.cassandra.write_batch_size",
      ConfigValue{ConfigType::Integer}.defaultValue(20).withConstraint(validateUint16)},
     {"etl_source.[].ip", Array{ConfigValue{ConfigType::String}.withConstraint(validateIP)}},
     {"etl_source.[].ws_port", Array{ConfigValue{ConfigType::String}.withConstraint(validatePort)}},
     {"etl_source.[].grpc_port", Array{ConfigValue{ConfigType::String}.withConstraint(validatePort)}},
     {"forwarding.cache_timeout",
      ConfigValue{ConfigType::Double}.defaultValue(0.0).withConstraint(validatePositiveDouble)},
     {"forwarding.request_timeout",
      ConfigValue{ConfigType::Double}.defaultValue(10.0).withConstraint(validatePositiveDouble)},
     {"dos_guard.whitelist.[]", Array{ConfigValue{ConfigType::String}}},
     {"dos_guard.max_fetches", ConfigValue{ConfigType::Integer}.defaultValue(1000'000).withConstraint(validateUint32)},
     {"dos_guard.max_connections", ConfigValue{ConfigType::Integer}.defaultValue(20).withConstraint(validateUint32)},
     {"dos_guard.max_requests", ConfigValue{ConfigType::Integer}.defaultValue(20).withConstraint(validateUint32)},
     {"dos_guard.sweep_interval",
      ConfigValue{ConfigType::Double}.defaultValue(1.0).withConstraint(validatePositiveDouble)},
     {"cache.peers.[].ip", Array{ConfigValue{ConfigType::String}.withConstraint(validateIP)}},
     {"cache.peers.[].port", Array{ConfigValue{ConfigType::String}.withConstraint(validatePort)}},
     {"server.ip", ConfigValue{ConfigType::String}.withConstraint(validateIP)},
     {"server.port", ConfigValue{ConfigType::Integer}.withConstraint(validatePort)},
     {"server.workers", ConfigValue{ConfigType::Integer}.withConstraint(validateUint32)},
     {"server.max_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(validateUint32)},
     {"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
     {"server.admin_password", ConfigValue{ConfigType::String}.optional()},
     {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
     {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
     {"io_threads", ConfigValue{ConfigType::Integer}.defaultValue(2).withConstraint(validateUint16)},
     {"cache.num_diffs", ConfigValue{ConfigType::Integer}.defaultValue(32).withConstraint(validateUint16)},
     {"cache.num_markers", ConfigValue{ConfigType::Integer}.defaultValue(48).withConstraint(validateUint16)},
     {"cache.num_cursors_from_diff", ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(validateUint16)},
     {"cache.num_cursors_from_account", ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(validateUint16)
     },
     {"cache.page_fetch_size", ConfigValue{ConfigType::Integer}.defaultValue(512).withConstraint(validateUint16)},
     {"cache.load", ConfigValue{ConfigType::String}.defaultValue("async").withConstraint(validateLoadMode)},
     {"log_channels.[].channel", Array{ConfigValue{ConfigType::String}.optional().withConstraint(validateChannelName)}},
     {"log_channels.[].log_level",
      Array{ConfigValue{ConfigType::String}.optional().withConstraint(validateLogLevelName)}},
     {"log_level", ConfigValue{ConfigType::String}.defaultValue("info").withConstraint(validateLogLevelName)},
     {"log_format",
      ConfigValue{ConfigType::String}.defaultValue(
          R"(%TimeStamp% (%SourceLocation%) [%ThreadID%] %Channel%:%Severity% %Message%)"
      )},
     {"log_to_console", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
     {"log_directory", ConfigValue{ConfigType::String}.optional()},
     {"log_rotation_size", ConfigValue{ConfigType::Integer}.defaultValue(2048u).withConstraint(validateUint32)},
     {"log_directory_max_size",
      ConfigValue{ConfigType::Integer}.defaultValue(50u * 1024u).withConstraint(validateUint32)},
     {"log_rotation_hour_interval", ConfigValue{ConfigType::Integer}.defaultValue(12).withConstraint(validateUint32)},
     {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint").withConstraint(validateLogTag)},
     {"extractor_threads", ConfigValue{ConfigType::Integer}.defaultValue(2u).withConstraint(validateUint32)},
     {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
     {"txn_threshold", ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(validateUint16)},
     {"start_sequence", ConfigValue{ConfigType::Integer}.optional().withConstraint(validateUint32)},
     {"finish_sequence", ConfigValue{ConfigType::Integer}.optional().withConstraint(validateUint32)},
     {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()},
     {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},
     {"api_version.min", ConfigValue{ConfigType::Integer}},
     {"api_version.max", ConfigValue{ConfigType::Integer}}}
};

ClioConfigDefinition::ClioConfigDefinition(std::initializer_list<KeyValuePair> pair)
{
    for (auto const& [key, value] : pair) {
        if (key.contains("[]"))
            ASSERT(std::holds_alternative<Array>(value), R"(Value must be array if key has "[]")");
        map_.insert({key, value});
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
        if (hasPrefix && !idx.has_value() && !mapKey.contains(prefixWithDot + "[]"))
            return ObjectView{prefix, *this};
    }
    ASSERT(false, "Key {} is not found in config", prefixWithDot);
    std::unreachable();
}

ArrayView
ClioConfigDefinition::getArray(std::string_view prefix) const
{
    auto const key = addBracketsForArrayKey(prefix);

    for (auto const& [mapKey, mapVal] : map_) {
        if (mapKey.starts_with(key)) {
            ASSERT(
                std::holds_alternative<Array>(mapVal), "Trying to retrieve Object or ConfigValue, instead of an Array "
            );
            return ArrayView{key, *this};
        }
    }
    ASSERT(false, "Key {} is not found in config", key);
    std::unreachable();
}

bool
ClioConfigDefinition::contains(std::string_view key) const
{
    return map_.contains(key);
}

bool
ClioConfigDefinition::hasItemsWithPrefix(std::string_view key) const
{
    return std::ranges::any_of(map_, [&key](auto const& pair) { return pair.first.starts_with(key); });
}

ValueView
ClioConfigDefinition::getValue(std::string_view fullKey) const
{
    ASSERT(map_.contains(fullKey), "key {} does not exist in config", fullKey);
    if (std::holds_alternative<ConfigValue>(map_.at(fullKey))) {
        return ValueView{std::get<ConfigValue>(map_.at(fullKey))};
    }
    ASSERT(false, "Value of key {} is an Array, not an object", fullKey);
    std::unreachable();
}

ValueView
ClioConfigDefinition::getValueInArray(std::string_view fullKey, std::size_t index) const
{
    auto const it = getArrayIterator(fullKey);
    return ValueView{std::get<Array>(it->second).at(index)};
}

Array const&
ClioConfigDefinition::asArray(std::string_view fullKey) const
{
    auto const it = getArrayIterator(fullKey);
    return std::get<Array>(it->second);
}

std::size_t
ClioConfigDefinition::arraySize(std::string_view prefix) const
{
    auto const key = addBracketsForArrayKey(prefix);

    for (auto const& pair : map_) {
        if (pair.first.starts_with(key)) {
            return std::get<Array>(pair.second).size();
        }
    }
    ASSERT(false, "Prefix {} not found in any of the config keys", key);
    std::unreachable();
}

std::optional<std::vector<Error>>
ClioConfigDefinition::parse(ConfigFileInterface const& config)
{
    std::vector<Error> listOfErrors;
    for (auto& [key, value] : map_) {
        // if key doesn't exist in user config, makes sure it is marked as ".optional()" or has ".defaultValue()"" in
        // ClioConfigDefitinion above
        if (!config.containsKey(key)) {
            if (std::holds_alternative<ConfigValue>(value)) {
                if (!(std::get<ConfigValue>(value).isOptional() || std::get<ConfigValue>(value).hasValue()))
                    listOfErrors.emplace_back(key, "key is required in user Config");
            } else if (std::holds_alternative<Array>(value)) {
                if (!(std::get<Array>(value).getArrayPattern().isOptional()))
                    listOfErrors.emplace_back(key, "key is required in user Config");
            }
            continue;
        }
        ASSERT(
            std::holds_alternative<ConfigValue>(value) || std::holds_alternative<Array>(value),
            "Value must be of type ConfigValue or Array"
        );
        std::visit(
            util::OverloadSet{// handle the case where the config value is a single element.
                              // attempt to set the value from the configuration for the specified key.
                              [&key, &config, &listOfErrors](ConfigValue& val) {
                                  if (auto const maybeError = val.setValue(config.getValue(key), key);
                                      maybeError.has_value())
                                      listOfErrors.emplace_back(maybeError.value());
                              },
                              // handle the case where the config value is an array.
                              // iterate over each provided value in the array and attempt to set it for the key.
                              [&key, &config, &listOfErrors](Array& arr) {
                                  for (auto const& val : config.getArray(key)) {
                                      if (auto const maybeError = arr.addValue(val, key); maybeError.has_value())
                                          listOfErrors.emplace_back(maybeError.value());
                                  }
                              }
            },
            value
        );
    }
    if (!listOfErrors.empty())
        return listOfErrors;

    return std::nullopt;
}

}  // namespace util::config
