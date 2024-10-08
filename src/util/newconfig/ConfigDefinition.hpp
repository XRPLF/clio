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

#pragma once

#include "rpc/common/APIVersion.hpp"
#include "util/Assert.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigDescription.hpp"
#include "util/newconfig/ConfigFileInterface.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Error.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/Types.hpp"
#include "util/newconfig/ValueView.hpp"

#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace util::config {

/**
 * @brief All the config data will be stored and extracted from this class
 *
 * Represents all the possible config data
 */
class ClioConfigDefinition {
public:
    using KeyValuePair = std::pair<std::string_view, std::variant<ConfigValue, Array>>;

    /**
     * @brief Constructs a new ClioConfigDefinition
     *
     * Initializes the configuration with a predefined set of key-value pairs
     * If a key contains "[]", the corresponding value must be an Array
     *
     * @param pair A list of key-value pairs for the predefined set of clio configurations
     */
    ClioConfigDefinition(std::initializer_list<KeyValuePair> pair);

    /**
     * @brief Parses the configuration file
     *
     * Also checks that no extra configuration key/value pairs are present. Adds to list of Errors
     * if it does
     *
     * @param config The configuration file interface
     * @return An optional vector of Error objects stating all the failures if parsing fails
     */
    [[nodiscard]] std::optional<std::vector<Error>>
    parse(ConfigFileInterface const& config);

    /**
     * @brief Validates the configuration file
     *
     * Should only check for valid values, without populating
     *
     * @param config The configuration file interface
     * @return An optional vector of Error objects stating all the failures if validation fails
     */
    [[nodiscard]] std::optional<std::vector<Error>>
    validate(ConfigFileInterface const& config) const;

    /**
     * @brief Generate markdown file of all the clio config descriptions
     *
     * @param configDescription The configuration description object
     * @return An optional Error if generating markdown fails
     */
    [[nodiscard]] std::expected<std::string, Error>
    getMarkdown(ClioConfigDescription const& configDescription) const;

    /**
     * @brief Returns the ObjectView specified with the prefix
     *
     * @param prefix The key prefix for the ObjectView
     * @param idx Used if getting Object in an Array
     * @return ObjectView with the given prefix
     */
    [[nodiscard]] ObjectView
    getObject(std::string_view prefix, std::optional<std::size_t> idx = std::nullopt) const;

    /**
     * @brief Returns the specified ValueView object associated with the key
     *
     * @param fullKey The config key to search for
     * @return ValueView associated with the given key
     */
    [[nodiscard]] ValueView
    getValue(std::string_view fullKey) const;

    /**
     * @brief Returns the specified ValueView object in an array with a given index
     *
     * @param fullKey The config key to search for
     * @param index The index of the config value inside the Array to get
     * @return ValueView associated with the given key
     */
    [[nodiscard]] ValueView
    getValueInArray(std::string_view fullKey, std::size_t index) const;

    /**
     * @brief Returns the specified Array object from ClioConfigDefinition
     *
     * @param prefix The prefix to search config keys from
     * @return ArrayView with all key-value pairs where key starts with "prefix"
     */
    [[nodiscard]] ArrayView
    getArray(std::string_view prefix) const;

    /**
     * @brief Checks if a key is present in the configuration map.
     *
     * @param key The key to search for in the configuration map.
     * @return True if the key is present, false otherwise.
     */
    [[nodiscard]] bool
    contains(std::string_view key) const;

    /**
     * @brief Checks if any key in config starts with "key"
     *
     * @param key The key to search for in the configuration map.
     * @return True if the any key in config starts with "key", false otherwise.
     */
    [[nodiscard]] bool
    hasItemsWithPrefix(std::string_view key) const;

    /**
     * @brief Returns the Array object associated with the specified key.
     *
     * @param key The key whose associated Array object is to be returned.
     * @return The Array object associated with the specified key.
     */
    [[nodiscard]] Array const&
    asArray(std::string_view key) const;

    /**
     * @brief Returns the size of an Array
     *
     * @param prefix The prefix whose associated Array object is to be returned.
     * @return The size of the array associated with the specified prefix.
     */
    [[nodiscard]] std::size_t
    arraySize(std::string_view prefix) const;

    /**
     * @brief Method to convert a float seconds value to milliseconds.
     *
     * @param value The value to convert
     * @return The value in milliseconds
     */
    static std::chrono::milliseconds
    toMilliseconds(float value);

    /**
     * @brief Returns an iterator to the beginning of the configuration map.
     *
     * @return A constant iterator to the beginning of the map.
     */
    [[nodiscard]] auto
    begin() const
    {
        return map_.begin();
    }

    /**
     * @brief Returns an iterator to the end of the configuration map.
     *
     * @return A constant iterator to the end of the map.
     */
    [[nodiscard]] auto
    end() const
    {
        return map_.end();
    }

private:
    /**
     * @brief returns the iterator of key-value pair with key fullKey
     *
     * @param fullKey Key to search for
     * @return iterator of map
     */
    [[nodiscard]] auto
    getArrayIterator(std::string_view key) const
    {
        auto const fullKey = addBracketsForArrayKey(key);
        auto const it = std::ranges::find_if(map_, [&fullKey](auto pair) { return pair.first == fullKey; });

        ASSERT(it != map_.end(), "key {} does not exist in config", fullKey);
        ASSERT(std::holds_alternative<Array>(it->second), "Value of {} is not an array", fullKey);

        return it;
    }

    /**
     * @brief Used for all Array API's; check to make sure "[]" exists, if not, append to end
     *
     * @param key key to check for
     * @return the key with "[]" appended to the end
     */
    [[nodiscard]] static std::string
    addBracketsForArrayKey(std::string_view key)
    {
        std::string fullKey = std::string(key);
        if (!key.contains(".[]"))
            fullKey += ".[]";
        return fullKey;
    }

    std::unordered_map<std::string_view, std::variant<ConfigValue, Array>> map_;
};

/**
 * @brief Full Clio Configuration definition.
 *
 * Specifies which keys are valid in Clio Config and provides default values if user's do not specify one. Those
 * without default values must be present in the user's config file.
 */
static ClioConfigDefinition ClioConfig = ClioConfigDefinition{
    {{"database.type", ConfigValue{ConfigType::String}.defaultValue("cassandra").withConstraint(validateCassandraName)},
     {"database.cassandra.contact_points", ConfigValue{ConfigType::String}.defaultValue("localhost")},
     {"database.cassandra.secure_connect_bundle", ConfigValue{ConfigType::String}.optional()},
     {"database.cassandra.port", ConfigValue{ConfigType::Integer}.withConstraint(validatePort)},
     {"database.cassandra.keyspace", ConfigValue{ConfigType::String}.defaultValue("clio")},
     {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(3u)},
     {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.optional()},
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
     {"database.cassandra.connect_timeout", ConfigValue{ConfigType::Integer}.withConstraint(validateUint32).optional()},
     {"database.cassandra.request_timeout", ConfigValue{ConfigType::Integer}.withConstraint(validateUint32).optional()},
     {"database.cassandra.username", ConfigValue{ConfigType::String}.optional()},
     {"database.cassandra.password", ConfigValue{ConfigType::String}.optional()},
     {"database.cassandra.certfile", ConfigValue{ConfigType::String}.optional()},
     {"allow_no_etl", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
     {"etl_sources.[].ip", Array{ConfigValue{ConfigType::String}.optional().withConstraint(validateIP)}},
     {"etl_sources.[].ws_port", Array{ConfigValue{ConfigType::String}.optional().withConstraint(validatePort)}},
     {"etl_sources.[].grpc_port", Array{ConfigValue{ConfigType::String}.optional().withConstraint(validatePort)}},
     {"forwarding.cache_timeout",
      ConfigValue{ConfigType::Double}.defaultValue(0.0).withConstraint(validatePositiveDouble)},
     {"forwarding.request_timeout",
      ConfigValue{ConfigType::Double}.defaultValue(10.0).withConstraint(validatePositiveDouble)},
     {"rpc.cache_timeout", ConfigValue{ConfigType::Double}.defaultValue(0.0).withConstraint(validatePositiveDouble)},
     {"num_markers", ConfigValue{ConfigType::Integer}.withConstraint(validateUint32).optional()},
     {"dos_guard.whitelist.[]", Array{ConfigValue{ConfigType::String}}},
     {"dos_guard.max_fetches", ConfigValue{ConfigType::Integer}.defaultValue(1000'000u).withConstraint(validateUint32)},
     {"dos_guard.max_connections", ConfigValue{ConfigType::Integer}.defaultValue(20u).withConstraint(validateUint32)},
     {"dos_guard.max_requests", ConfigValue{ConfigType::Integer}.defaultValue(20u).withConstraint(validateUint32)},
     {"dos_guard.sweep_interval",
      ConfigValue{ConfigType::Double}.defaultValue(1.0).withConstraint(validatePositiveDouble)},
     {"workers",
      ConfigValue{ConfigType::Integer}.defaultValue(std::thread::hardware_concurrency()).withConstraint(validateUint32)
     },
     {"server.ip", ConfigValue{ConfigType::String}.withConstraint(validateIP)},
     {"server.port", ConfigValue{ConfigType::Integer}.withConstraint(validatePort)},
     {"server.max_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(validateUint32)},
     {"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
     {"server.admin_password", ConfigValue{ConfigType::String}.optional()},
     {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
     {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
     {"io_threads", ConfigValue{ConfigType::Integer}.defaultValue(2).withConstraint(validateUint16)},
     {"subscription_workers", ConfigValue{ConfigType::Integer}.defaultValue(1).withConstraint(validateUint32)},
     {"graceful_period", ConfigValue{ConfigType::Double}.defaultValue(10.0).withConstraint(validatePositiveDouble)},
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
     {"log_rotation_size", ConfigValue{ConfigType::Integer}.defaultValue(2048).withConstraint(validateUint32)},
     {"log_directory_max_size", ConfigValue{ConfigType::Integer}.defaultValue(50 * 1024).withConstraint(validateUint32)
     },
     {"log_rotation_hour_interval", ConfigValue{ConfigType::Integer}.defaultValue(12).withConstraint(validateUint32)},
     {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("none").withConstraint(validateLogTag)},
     {"extractor_threads", ConfigValue{ConfigType::Integer}.defaultValue(1u).withConstraint(validateUint32)},
     {"read_only", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
     {"txn_threshold", ConfigValue{ConfigType::Integer}.defaultValue(0).withConstraint(validateUint16)},
     {"start_sequence", ConfigValue{ConfigType::Integer}.optional().withConstraint(validateUint32)},
     {"finish_sequence", ConfigValue{ConfigType::Integer}.optional().withConstraint(validateUint32)},
     {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()},
     {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},
     {"api_version.default",
      ConfigValue{ConfigType::Integer}.defaultValue(rpc::API_VERSION_DEFAULT).withConstraint(validateApiVersion)},
     {"api_version.min",
      ConfigValue{ConfigType::Integer}.defaultValue(rpc::API_VERSION_MIN).withConstraint(validateApiVersion)},
     {"api_version.max",
      ConfigValue{ConfigType::Integer}.defaultValue(rpc::API_VERSION_MAX).withConstraint(validateApiVersion)}}
};

}  // namespace util::config
