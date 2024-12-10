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

#include "util/Assert.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace util::config {

/**
 * @brief All the config description are stored and extracted from this class
 *
 * Represents all the possible config description
 */
struct ClioConfigDescription {
public:
    /** @brief Struct to represent a key-value pair*/
    struct KV {
        std::string_view key;
        std::string_view value;
    };

    /**
     * @brief Constructs a new Clio Config Description based on pre-existing descriptions
     *
     * Config Keys and it's corresponding descriptions are all predefined. Used to generate markdown file
     */
    constexpr ClioConfigDescription() = default;

    /**
     * @brief Retrieves the description for a given key
     *
     * @param key The key to look up the description for
     * @return The description associated with the key, or "Not Found" if the key does not exist
     */
    [[nodiscard]] static constexpr std::string_view
    get(std::string_view key)
    {
        auto const itr = std::ranges::find_if(configDescription, [&](auto const& v) { return v.key == key; });
        ASSERT(itr != configDescription.end(), "Key {} doesn't exist in config", key);
        return itr->value;
    }

private:
    static constexpr auto configDescription = std::array{
        KV{.key = "database.type", .value = "Type of database to use."},
        KV{.key = "database.cassandra.contact_points",
           .value = "Comma-separated list of contact points for Cassandra nodes."},
        KV{.key = "database.cassandra.port", .value = "Port number to connect to Cassandra."},
        KV{.key = "database.cassandra.keyspace", .value = "Keyspace to use in Cassandra."},
        KV{.key = "database.cassandra.replication_factor", .value = "Number of replicated nodes for Scylladb."},
        KV{.key = "database.cassandra.table_prefix", .value = "Prefix for Cassandra table names."},
        KV{.key = "database.cassandra.max_write_requests_outstanding",
           .value = "Maximum number of outstanding write requests."},
        KV{.key = "database.cassandra.max_read_requests_outstanding",
           .value = "Maximum number of outstanding read requests."},
        KV{.key = "database.cassandra.threads", .value = "Number of threads for Cassandra operations."},
        KV{.key = "database.cassandra.core_connections_per_host",
           .value = "Number of core connections per host for Cassandra."},
        KV{.key = "database.cassandra.queue_size_io", .value = "Queue size for I/O operations in Cassandra."},
        KV{.key = "database.cassandra.write_batch_size", .value = "Batch size for write operations in Cassandra."},
        KV{.key = "etl_source.[].ip", .value = "IP address of the ETL source."},
        KV{.key = "etl_source.[].ws_port", .value = "WebSocket port of the ETL source."},
        KV{.key = "etl_source.[].grpc_port", .value = "gRPC port of the ETL source."},
        KV{.key = "forwarding.cache_timeout",
           .value = "Timeout duration for the forwarding cache used in Rippled communication."},
        KV{.key = "forwarding.request_timeout",
           .value = "Timeout duration for the forwarding request used in Rippled communication."},
        KV{.key = "dos_guard.[].whitelist", .value = "List of IP addresses to whitelist for DOS protection."},
        KV{.key = "dos_guard.max_fetches", .value = "Maximum number of fetch operations allowed by DOS guard."},
        KV{.key = "dos_guard.max_connections", .value = "Maximum number of concurrent connections allowed by DOS guard."
        },
        KV{.key = "dos_guard.max_requests", .value = "Maximum number of requests allowed by DOS guard."},
        KV{.key = "dos_guard.sweep_interval", .value = "Interval in seconds for DOS guard to sweep/clear its state."},
        KV{.key = "cache.peers.[].ip", .value = "IP address of peer nodes to cache."},
        KV{.key = "cache.peers.[].port", .value = "Port number of peer nodes to cache."},
        KV{.key = "server.ip", .value = "IP address of the Clio HTTP server."},
        KV{.key = "server.port", .value = "Port number of the Clio HTTP server."},
        KV{.key = "server.max_queue_size", .value = "Maximum size of the server's request queue."},
        KV{.key = "server.workers", .value = "Maximum number of threads for server to run with."},
        KV{.key = "server.local_admin", .value = "Indicates if the server should run with admin privileges."},
        KV{.key = "server.admin_password", .value = "Password for Clio admin-only APIs."},
        KV{.key = "prometheus.enabled", .value = "Enable or disable Prometheus metrics."},
        KV{.key = "prometheus.compress_reply", .value = "Enable or disable compression of Prometheus responses."},
        KV{.key = "io_threads", .value = "Number of I/O threads."},
        KV{.key = "cache.num_diffs", .value = "Number of diffs to cache."},
        KV{.key = "cache.num_markers", .value = "Number of markers to cache."},
        KV{.key = "cache.num_cursors_from_diff", .value = "Num of cursors that are different."},
        KV{.key = "cache.num_cursors_from_account", .value = "Number of cursors from an account."},
        KV{.key = "cache.page_fetch_size", .value = "Page fetch size for cache operations."},
        KV{.key = "cache.load", .value = "Cache loading strategy ('sync' or 'async')."},
        KV{.key = "log_channels.[].channel", .value = "Name of the log channel."},
        KV{.key = "log_channels.[].log_level", .value = "Log level for the log channel."},
        KV{.key = "log_level", .value = "General logging level of Clio."},
        KV{.key = "log_format", .value = "Format string for log messages."},
        KV{.key = "log_to_console", .value = "Enable or disable logging to console."},
        KV{.key = "log_directory", .value = "Directory path for log files."},
        KV{.key = "log_rotation_size", .value = "Log rotation size in megabytes."},
        KV{.key = "log_directory_max_size", .value = "Maximum size of the log directory in megabytes."},
        KV{.key = "log_rotation_hour_interval", .value = "Interval in hours for log rotation."},
        KV{.key = "log_tag_style", .value = "Style for log tags."},
        KV{.key = "extractor_threads", .value = "Number of extractor threads."},
        KV{.key = "read_only", .value = "Indicates if the server should have read-only privileges."},
        KV{.key = "txn_threshold", .value = "Transaction threshold value."},
        KV{.key = "start_sequence", .value = "Starting ledger index."},
        KV{.key = "finish_sequence", .value = "Ending ledger index."},
        KV{.key = "ssl_cert_file", .value = "Path to the SSL certificate file."},
        KV{.key = "ssl_key_file", .value = "Path to the SSL key file."},
        KV{.key = "api_version.min", .value = "Minimum API version."},
        KV{.key = "api_version.max", .value = "Maximum API version."}
    };
};

}  // namespace util::config
