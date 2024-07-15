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

#include <cstddef>
#include <initializer_list>
#include <optional>
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
     {"database.cassandra.replication_factor", ConfigValue{ConfigType::Integer}.defaultValue(3)},
     {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.defaultValue("table_prefix")},
     {"database.cassandra.max_write_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(10'000)},
     {"database.cassandra.max_read_requests_outstanding", ConfigValue{ConfigType::Integer}.defaultValue(100'000)},
     {"database.cassandra.threads",
      ConfigValue{ConfigType::Integer}.defaultValue(static_cast<int>(std::thread::hardware_concurrency()))},
     {"database.cassandra.core_connections_per_host", ConfigValue{ConfigType::Integer}.defaultValue(1)},
     {"database.cassandra.queue_size_io", ConfigValue{ConfigType::Integer}.optional()},
     {"database.cassandra.write_batch_size", ConfigValue{ConfigType::Integer}.defaultValue(20)},
     {"etl_source.[].ip", Array{ConfigValue{ConfigType::String}.optional()}},
     {"etl_source.[].ws_port", Array{ConfigValue{ConfigType::String}.min(1).max(65535)}},
     {"etl_source.[].grpc_port", Array{ConfigValue{ConfigType::String}.min(1).max(65535)}},
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
     {"extractor_threads", ConfigValue{ConfigType::Integer}.defaultValue(2)},
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
ClioConfigDefinition::getObject(std::string_view prefix, std::optional<std::size_t> idx) const
{
    std::string prefixWithDot = std::string(prefix) + ".";
    for (auto const& [mapKey, mapVal] : map_) {
        if (mapKey.starts_with(prefixWithDot))
            ASSERT(!mapKey.ends_with(".[]"), "Trying to retrieve an object when value is an Array");

        if (mapKey.starts_with(prefixWithDot) && std::holds_alternative<ConfigValue>(mapVal))
            return ObjectView{prefix, *this};

        if (mapKey.starts_with(prefixWithDot) && std::holds_alternative<Array>(mapVal)) {
            ASSERT(std::get<Array>(mapVal).size() > idx, "index provided is out of scope");
            return ObjectView{prefix, idx.value(), *this};
        }
    }
    throw std::invalid_argument("Key is not found in config");
}

ArrayView
ClioConfigDefinition::getArray(std::string_view prefix) const
{
    std::string key = std::string(prefix);
    if (!prefix.contains(".[]"))
        key += ".[]";

    for (auto const& [mapKey, mapVal] : map_) {
        if (mapKey.starts_with(key))
            return ArrayView{key, *this};
        ASSERT(!mapKey.starts_with(prefix), "Trying to retrieve an Array when value is an object");
    }
    throw std::invalid_argument("Key is not found in config");
}

ValueView
ClioConfigDefinition::getValue(std::string_view fullKey) const
{
    if (map_.contains(fullKey) && std::holds_alternative<ConfigValue>(map_.at(fullKey))) {
        return ValueView{std::get<ConfigValue>(map_.at(fullKey))};
    }
    ASSERT(
        !map_.contains(fullKey) && std::holds_alternative<Array>(map_.at(fullKey)), "value of key is not Config Value."
    );

    throw std::invalid_argument("no matching key");
}

/**
 * @brief Description of each config key and what they mean. Used to generate markdown file
 *
 * Key-value pairs. Key is configKey, value is its matching description
 * Maybe_unused will be removed when markdown file is generated
 */
[[maybe_unused]] static constexpr ClioConfigDescription const DESCRIPTIONS{
    {{"database.type", "Type of database to use."},
     {"database.cassandra.contact_points", "Comma-separated list of contact points for Cassandra nodes."},
     {"database.cassandra.port", "Port number to connect to Cassandra."},
     {"database.cassandra.keyspace", "Keyspace to use in Cassandra."},
     {"database.cassandra.replication_factor", "Number of replicated nodes for Scylladb."},
     {"database.cassandra.table_prefix", "Prefix for Cassandra table names."},
     {"database.cassandra.max_write_requests_outstanding", "Maximum number of outstanding write requests."},
     {"database.cassandra.max_read_requests_outstanding", "Maximum number of outstanding read requests."},
     {"database.cassandra.threads", "Number of threads for Cassandra operations."},
     {"database.cassandra.core_connections_per_host", "Number of core connections per host for Cassandra."},
     {"database.cassandra.queue_size_io", "Queue size for I/O operations in Cassandra."},
     {"database.cassandra.write_batch_size", "Batch size for write operations in Cassandra."},
     {"etl_source.[].ip", "IP address of the ETL source."},
     {"etl_source.[].ws_port", "WebSocket port of the ETL source."},
     {"etl_source.[].grpc_port", "gRPC port of the ETL source."},
     {"forwarding_cache_timeout", "Timeout duration for the forwarding cache used in Rippled communication."},
     {"dos_guard.[].whitelist", "List of IP addresses to whitelist for DOS protection."},
     {"dos_guard.max_fetches", "Maximum number of fetch operations allowed by DOS guard."},
     {"dos_guard.max_connections", "Maximum number of concurrent connections allowed by DOS guard."},
     {"dos_guard.max_requests", "Maximum number of requests allowed by DOS guard."},
     {"dos_guard.sweep_interval", "Interval in seconds for DOS guard to sweep/clear its state."},
     {"cache.peers.[].ip", "IP address of peer nodes to cache."},
     {"cache.peers.[].port", "Port number of peer nodes to cache."},
     {"server.ip", "IP address of the Clio HTTP server."},
     {"server.port", "Port number of the Clio HTTP server."},
     {"server.max_queue_size", "Maximum size of the server's request queue."},
     {"server.local_admin", "Indicates if the server should run with admin privileges."},
     {"prometheus.enabled", "Enable or disable Prometheus metrics."},
     {"prometheus.compress_reply", "Enable or disable compression of Prometheus responses."},
     {"io_threads", "Number of I/O threads."},
     {"cache.num_diffs", "Number of diffs to cache."},
     {"cache.num_markers", "Number of markers to cache."},
     {"cache.page_fetch_size", "Page fetch size for cache operations."},
     {"cache.load", "Cache loading strategy ('sync' or 'async')."},
     {"log_channels.[].channel", "Name of the log channel."},
     {"log_channels.[].log_level", "Log level for the log channel."},
     {"log_level", "General logging level of Clio."},
     {"log_format", "Format string for log messages."},
     {"log_to_console", "Enable or disable logging to console."},
     {"log_directory", "Directory path for log files."},
     {"log_rotation_size", "Log rotation size in megabytes."},
     {"log_directory_max_size", "Maximum size of the log directory in megabytes."},
     {"log_rotation_hour_interval", "Interval in hours for log rotation."},
     {"log_tag_style", "Style for log tags."},
     {"extractor_threads", "Number of extractor threads."},
     {"read_only", "Indicates if the server should have read-only privileges."},
     {"txn_threshold", "Transaction threshold value."},
     {"start_sequence", "Starting ledger index."},
     {"finish_sequence", "Ending ledger index."},
     {"ssl_cert_file", "Path to the SSL certificate file."},
     {"ssl_key_file", "Path to the SSL key file."},
     {"api_version.min", "Minimum API version."},
     {"api_version.max", "Maximum API version."}}
};

}  // namespace util::config
