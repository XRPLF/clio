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
#include "util/newconfig/Errors.hpp"
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
    {{"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra").withConstraint(nameCassandra)},
     {"database.cassandra.contact_points", ConfigValue{ConfigType::String}.defaultValue("localhost")},
     {"database.cassandra.port", ConfigValue{ConfigType::Integer}.withConstraint(port)},
     {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue("clio")},
     {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(3u)},
     {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.defaultValue("table_prefix")},
     {"database.cassandra.max_write_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(10'000)},
     {"database.cassandra.max_read_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(100'000)},
     {"database.cassandra.threads",
      ConfigValue{ConfigType::Integer}.defaultValue(static_cast<uint32_t>(std::thread::hardware_concurrency()))},
     {"database.cassandra.core_connections_per_host",
      ConfigValue{ConfigType::Integer}.defaultValue(1).withConstraint(uint16)},
     {"database.cassandra.queue_size_io", ConfigValue{ConfigType::Integer}.optional().withConstraint(uint16)},
     {"database.cassandra.write_batch_size", ConfigValue{ConfigType::Integer}.defaultValue(20).withConstraint(uint16)},
     {"etl_source.[].ip", Array{ConfigValue{ConfigType::String}.optional().withConstraint(validIP)}},
     {"etl_source.[].ws_port", Array{ConfigValue{ConfigType::String}.optional().withConstraint(port)}},
     {"etl_source.[].grpc_port", Array{ConfigValue{ConfigType::String}.optional().withConstraint(port)}},
     {"forwarding.cache_timeout", ConfigValue{ConfigType::Double}.defaultValue(0.0).withConstraint(uint16)},
     {"forwarding.request_timeout", ConfigValue{ConfigType::Double}.defaultValue(10.0).withConstraint(uint16)},
     {"dos_guard.whitelist.[]", Array{ConfigValue{ConfigType::String}}},
     {"dos_guard.max_fetches", ConfigValue{ConfigType::Integer}.defaultValue(1000'000).withConstraint(uint16)},
     {"dos_guard.max_connections", ConfigValue{ConfigType::Integer}.defaultValue(20).withConstraint(uint16)},
     {"dos_guard.max_requests", ConfigValue{ConfigType::Integer}.defaultValue(20).withConstraint(uint16)},
     {"dos_guard.sweep_interval", ConfigValue{ConfigType::Double}.defaultValue(1.0)},
     {"cache.peers.[].ip", Array{ConfigValue{ConfigType::String}.withConstraint(validIP)}},
     {"cache.peers.[].port", Array{ConfigValue{ConfigType::String}.withConstraint(port)}},
     {"server.ip", ConfigValue{ConfigType::String}.withConstraint(validIP)},
     {"server.port", ConfigValue{ConfigType::Integer}.withConstraint(port)},
     {"server.workers", ConfigValue{ConfigType::Integer}.withConstraint(uint32)},
     {"server.max_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(uint32)},
     {"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
     {"server.admin_password", ConfigValue{ConfigType::String}.optional()},
     {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
     {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
     {"io_threads", ConfigValue{ConfigType::Integer}.defaultValue(2).withConstraint(uint16)},
     {"cache.num_diffs", ConfigValue{ConfigType::Integer}.defaultValue(32).withConstraint(uint16)},
     {"cache.num_markers", ConfigValue{ConfigType::Integer}.defaultValue(48).withConstraint(uint16)},
     {"cache.num_cursors_from_diff", ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(uint16)},
     {"cache.num_cursors_from_account", ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(uint16)},
     {"cache.page_fetch_size", ConfigValue{ConfigType::Integer}.defaultValue(512).withConstraint(uint16)},
     {"cache.load", ConfigValue{ConfigType::String}.defaultValue("async").withConstraint(loadMode)},
     {"log_channels.[].channel", Array{ConfigValue{ConfigType::String}.optional().withConstraint(channelName)}},
     {"log_channels.[].log_level", Array{ConfigValue{ConfigType::String}.optional().withConstraint(logLevelName)}},
     {"log_level", ConfigValue{ConfigType::String}.defaultValue("info").withConstraint(logLevelName)},
     {"log_format",
      ConfigValue{ConfigType::String}.defaultValue(
          R"(%TimeStamp% (%SourceLocation%) [%ThreadID%] %Channel%:%Severity% %Message%)"
      )},
     {"log_to_console", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
     {"log_directory", ConfigValue{ConfigType::String}.optional()},
     {"log_rotation_size", ConfigValue{ConfigType::Integer}.defaultValue(2048).withConstraint(uint64)},
     {"log_directory_max_size", ConfigValue{ConfigType::Integer}.defaultValue(50 * 1024).withConstraint(uint64)},
     {"log_rotation_hour_interval", ConfigValue{ConfigType::Integer}.defaultValue(12).withConstraint(uint32)},
     {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint").withConstraint(logTag)},
     {"extractor_threads", ConfigValue{ConfigType::Integer}.defaultValue(2u).withConstraint(uint32)},
     {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
     {"txn_threshold", ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(uint16)},
     {"start_sequence", ConfigValue{ConfigType::Integer}.optional().withConstraint(uint32)},
     {"finish_sequence", ConfigValue{ConfigType::Integer}.optional().withConstraint(uint32)},
     {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()},
     {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},
     {"api_version.min", ConfigValue{ConfigType::Integer}},
     {"api_version.max", ConfigValue{ConfigType::Integer}}}
};

ClioConfigDefinition::ClioConfigDefinition(std::initializer_list<KeyValuePair> pair)
{
    for (auto const& [key, value] : pair) {
        if (key.contains("[]"))
            ASSERT(std::holds_alternative<Array>(value), "Value must be array if key has \"[]\"");
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
                    listOfErrors.emplace_back(fmt::format("Key {} is required in user Config", key));
            } else if (std::holds_alternative<Array>(value)) {
                for (auto const& configVal : std::get<Array>(value)) {
                    if (!(configVal.isOptional() || configVal.hasValue()))
                        listOfErrors.emplace_back(fmt::format("Key {} is required in user Config", key));
                }
            }
            continue;
        }
        ASSERT(
            std::holds_alternative<ConfigValue>(value) || std::holds_alternative<Array>(value),
            "Value must be of type ConfigValue or Array"
        );
        std::visit(
            util::OverloadSet{
                [&key, &config, &listOfErrors](ConfigValue& value) {
                    if (auto const setVal = value.setValue(config.getValue(key)); setVal.has_value())
                        listOfErrors.emplace_back(setVal.value());
                },
                // All configValues in Array gotten from user must have same type and constraint as the first element in
                // Array specified ClioConfigDefinition up top
                [&key, &config, &listOfErrors](Array& arr) {
                    auto const firstVal = arr.at(0);
                    auto const constraint = firstVal.getConstraint();

                    for (auto const& val : config.getArray(key)) {
                        ConfigValue configVal{firstVal.type()};
                        auto const resultOfSettingValue = constraint.has_value()
                            ? configVal.withConstraint(constraint.value()).setValue(val)
                            : configVal.setValue(val);

                        if (resultOfSettingValue.has_value()) {
                            listOfErrors.emplace_back(resultOfSettingValue.value());
                        } else {
                            arr.emplaceBack(std::move(configVal));
                        }
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
