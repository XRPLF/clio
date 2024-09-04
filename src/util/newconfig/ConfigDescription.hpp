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
        KV{"database.type", "Type of database to use."},
        KV{"database.cassandra.contact_points", "Comma-separated list of contact points for Cassandra nodes."},
        KV{"database.cassandra.port", "Port number to connect to Cassandra."},
        KV{"database.cassandra.keyspace", "Keyspace to use in Cassandra."},
        KV{"database.cassandra.replication_factor", "Number of replicated nodes for Scylladb."},
        KV{"database.cassandra.table_prefix", "Prefix for Cassandra table names."},
        KV{"database.cassandra.max_write_requests_outstanding", "Maximum number of outstanding write requests."},
        KV{"database.cassandra.max_read_requests_outstanding", "Maximum number of outstanding read requests."},
        KV{"database.cassandra.threads", "Number of threads for Cassandra operations."},
        KV{"database.cassandra.core_connections_per_host", "Number of core connections per host for Cassandra."},
        KV{"database.cassandra.queue_size_io", "Queue size for I/O operations in Cassandra."},
        KV{"database.cassandra.write_batch_size", "Batch size for write operations in Cassandra."},
        KV{"etl_sources.[].ip", "IP address of the ETL source."},
        KV{"etl_sources.[].ws_port", "WebSocket port of the ETL source."},
        KV{"etl_sources.[].grpc_port", "gRPC port of the ETL source."},
        KV{"forwarding.cache_timeout", "Timeout duration for the forwarding cache used in Rippled communication."},
        KV{"forwarding.request_timeout", "Timeout duration for the forwarding request used in Rippled communication."},
        KV{"dos_guard.[].whitelist", "List of IP addresses to whitelist for DOS protection."},
        KV{"dos_guard.max_fetches", "Maximum number of fetch operations allowed by DOS guard."},
        KV{"dos_guard.max_connections", "Maximum number of concurrent connections allowed by DOS guard."},
        KV{"dos_guard.max_requests", "Maximum number of requests allowed by DOS guard."},
        KV{"dos_guard.sweep_interval", "Interval in seconds for DOS guard to sweep/clear its state."},
        KV{"cache.peers.[].ip", "IP address of peer nodes to cache."},
        KV{"cache.peers.[].port", "Port number of peer nodes to cache."},
        KV{"server.ip", "IP address of the Clio HTTP server."},
        KV{"server.port", "Port number of the Clio HTTP server."},
        KV{"server.max_queue_size", "Maximum size of the server's request queue."},
        KV{"server.workers", "Maximum number of threads for server to run with."},
        KV{"server.local_admin", "Indicates if the server should run with admin privileges."},
        KV{"server.admin_password", "Password required to run Clio Server with admin privileges"},
        KV{"prometheus.enabled", "Enable or disable Prometheus metrics."},
        KV{"prometheus.compress_reply", "Enable or disable compression of Prometheus responses."},
        KV{"io_threads", "Number of I/O threads."},
        KV{"cache.num_diffs", "Number of diffs to cache."},
        KV{"cache.num_markers", "Number of markers to cache."},
        KV{"cache.num_cursors_from_diff", "Num of cursors that are different."},
        KV{"cache.num_cursors_from_account", "Number of cursors from an account."},
        KV{"cache.page_fetch_size", "Page fetch size for cache operations."},
        KV{"cache.load", "Cache loading strategy ('sync' or 'async')."},
        KV{"log_channels.[].channel", "Name of the log channel."},
        KV{"log_channels.[].log_level", "Log level for the log channel."},
        KV{"log_level", "General logging level of Clio."},
        KV{"log_format", "Format string for log messages."},
        KV{"log_to_console", "Enable or disable logging to console."},
        KV{"log_directory", "Directory path for log files."},
        KV{"log_rotation_size", "Log rotation size in megabytes."},
        KV{"log_directory_max_size", "Maximum size of the log directory in megabytes."},
        KV{"log_rotation_hour_interval", "Interval in hours for log rotation."},
        KV{"log_tag_style", "Style for log tags."},
        KV{"extractor_threads", "Number of extractor threads."},
        KV{"read_only", "Indicates if the server should have read-only privileges."},
        KV{"txn_threshold", "Transaction threshold value."},
        KV{"start_sequence", "Starting ledger index."},
        KV{"finish_sequence", "Ending ledger index."},
        KV{"ssl_cert_file", "Path to the SSL certificate file."},
        KV{"ssl_key_file", "Path to the SSL key file."},
        KV{"api_version.min", "Minimum API version."},
        KV{"api_version.max", "Maximum API version."}
    };
};

}  // namespace util::config
