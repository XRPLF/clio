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
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/Error.hpp"

#include <fmt/core.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
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
        auto const itr = std::ranges::find_if(kCONFIG_DESCRIPTION, [&](auto const& v) { return v.key == key; });
        ASSERT(itr != kCONFIG_DESCRIPTION.end(), "Key {} doesn't exist in config", key);
        return itr->value;
    }

    /**
     * @brief Generate markdown file of all the clio config descriptions
     *
     * @param path The path location to generate the Config-description file
     * @return An Error if generating markdown fails, otherwise nothing
     */
    [[nodiscard]] static std::expected<void, Error>
    generateConfigDescriptionToFile(std::filesystem::path path)
    {
        namespace fs = std::filesystem;

        // Validate the directory exists
        auto const dir = path.parent_path();
        if (!dir.empty() && !fs::exists(dir)) {
            return std::unexpected<Error>{
                fmt::format("Error: Directory '{}' does not exist or provided path is invalid", dir.string())
            };
        }

        std::ofstream file(path.string());
        if (!file.is_open()) {
            return std::unexpected{fmt::format("Failed to create file '{}': {}", path.string(), std::strerror(errno))};
        }

        writeConfigDescriptionToFile(file);
        file.close();

        std::cout << "Markdown file generated successfully: " << path << "\n";
        return {};
    }

    /**
     * @brief Writes to Config description to file
     *
     * @param file The config file to write to
     */
    static void
    writeConfigDescriptionToFile(std::ostream& file)
    {
        file << "# Clio Config Description\n";
        file << "This file lists all Clio Configuration definitions in detail.\n\n";
        file << "## Configuration Details\n\n";

        for (auto const& [key, val] : kCONFIG_DESCRIPTION) {
            file << "### Key: " << key << "\n";

            // Every type of value is directed to operator<< in ConfigValue.hpp
            // as ConfigValue is the one that holds all the info regarding the config values
            if (key.contains("[]")) {
                file << gClioConfig.asArray(key);
            } else {
                file << gClioConfig.getValueView(key);
            }
            file << " -   **Description**: " << val << "\n";
        }
        file << "\n";
    }

private:
    static constexpr auto kCONFIG_DESCRIPTION = std::array{
        KV{.key = "database.type",
           .value = "Type of database to use. We currently support Cassandra and Scylladb. We default to Scylladb."},
        KV{.key = "database.cassandra.contact_points",
           .value = "A list of IP addresses or hostnames of the initial nodes (Cassandra/Scylladb cluster nodes) that "
                    "the client will connect to when establishing a connection with the database. If you're running "
                    "locally, it should be 'localhost' or 127.0.0.1"},
        KV{.key = "database.cassandra.secure_connect_bundle",
           .value = "Configuration file that contains the necessary security credentials and connection details for "
                    "securely "
                    "connecting to a Cassandra database cluster."},
        KV{.key = "database.cassandra.port", .value = "Port number to connect to the database."},
        KV{.key = "database.cassandra.keyspace", .value = "Keyspace to use for the database."},
        KV{.key = "database.cassandra.replication_factor",
           .value = "Number of replicated nodes for Scylladb. Visit this link for more details : "
                    "https://university.scylladb.com/courses/scylla-essentials-overview/lessons/high-availability/"
                    "topic/fault-tolerance-replication-factor/ "},
        KV{.key = "database.cassandra.table_prefix", .value = "Prefix for Database table names."},
        KV{.key = "database.cassandra.max_write_requests_outstanding",
           .value = "Maximum number of outstanding write requests. Write requests are api calls that write to database "
        },
        KV{.key = "database.cassandra.max_read_requests_outstanding",
           .value = "Maximum number of outstanding read requests, which reads from database"},
        KV{.key = "database.cassandra.threads", .value = "Number of threads that will be used for database operations."
        },
        KV{.key = "database.cassandra.core_connections_per_host",
           .value = "Number of core connections per host for Cassandra."},
        KV{.key = "database.cassandra.queue_size_io", .value = "Queue size for I/O operations in Cassandra."},
        KV{.key = "database.cassandra.write_batch_size", .value = "Batch size for write operations in Cassandra."},
        KV{.key = "database.cassandra.connect_timeout",
           .value = "The maximum amount of time in seconds the system will wait for a connection to be successfully "
                    "established "
                    "with the database."},
        KV{.key = "database.cassandra.request_timeout",
           .value =
               "The maximum amount of time in seconds the system will wait for a request to be fetched from database."},
        KV{.key = "database.cassandra.username", .value = "The username used for authenticating with the database."},
        KV{.key = "database.cassandra.password", .value = "The password used for authenticating with the database."},
        KV{.key = "database.cassandra.certfile",
           .value = "The path to the SSL/TLS certificate file used to establish a secure connection between the client "
                    "and the "
                    "Cassandra database."},
        KV{.key = "allow_no_etl", .value = "If True, no ETL nodes will run with Clio."},
        KV{.key = "etl_sources.[].ip", .value = "IP address of the ETL source."},
        KV{.key = "etl_sources.[].ws_port", .value = "WebSocket port of the ETL source."},
        KV{.key = "etl_sources.[].grpc_port", .value = "gRPC port of the ETL source."},
        KV{.key = "forwarding.cache_timeout",
           .value = "Timeout duration for the forwarding cache used in Rippled communication."},
        KV{.key = "forwarding.request_timeout",
           .value = "Timeout duration for the forwarding request used in Rippled communication."},
        KV{.key = "rpc.cache_timeout", .value = "Timeout duration for RPC requests."},
        KV{.key = "num_markers",
           .value = "The number of markers is the number of coroutines to download the initial ledger"},
        KV{.key = "dos_guard.whitelist.[]", .value = "List of IP addresses to whitelist for DOS protection."},
        KV{.key = "dos_guard.max_fetches", .value = "Maximum number of fetch operations allowed by DOS guard."},
        KV{.key = "dos_guard.max_connections", .value = "Maximum number of concurrent connections allowed by DOS guard."
        },
        KV{.key = "dos_guard.max_requests", .value = "Maximum number of requests allowed by DOS guard."},
        KV{.key = "dos_guard.sweep_interval", .value = "Interval in seconds for DOS guard to sweep/clear its state."},
        KV{.key = "workers", .value = "Number of threads to process RPC requests."},
        KV{.key = "server.ip", .value = "IP address of the Clio HTTP server."},
        KV{.key = "server.port", .value = "Port number of the Clio HTTP server."},
        KV{.key = "server.max_queue_size",
           .value = "Maximum size of the server's request queue. Value of 0 is no limit."},
        KV{.key = "server.local_admin",
           .value = "Indicates if the server should run with admin privileges. Only one of local_admin or "
                    "admin_password can be set."},
        KV{.key = "server.admin_password",
           .value = "Password for Clio admin-only APIs. Only one of local_admin or admin_password can be set."},
        KV{.key = "server.processing_policy",
           .value = R"(Could be "sequent" or "parallel". For the sequent policy, requests from a single client 
        connection are processed one by one, with the next request read only after the previous one is processed. For the parallel policy, Clio will accept
         all requests and process them in parallel, sending a reply for each request as soon as it is ready.)"},
        KV{.key = "server.parallel_requests_limit",
           .value =
               R"(Optional parameter, used only if processing_strategy `parallel`. It limits the number of requests for a single client connection that are processed in parallel. If not specified, the limit is infinite.)"
        },
        KV{.key = "server.ws_max_sending_queue_size", .value = "Maximum size of the websocket sending queue."},
        KV{.key = "prometheus.enabled", .value = "Enable or disable Prometheus metrics."},
        KV{.key = "prometheus.compress_reply", .value = "Enable or disable compression of Prometheus responses."},
        KV{.key = "io_threads", .value = "Number of I/O threads. Value cannot be less than 1"},
        KV{.key = "subscription_workers",
           .value = "The number of worker threads or processes that are responsible for managing and processing "
                    "subscription-based tasks from rippled"},
        KV{.key = "graceful_period", .value = "Number of milliseconds server will wait to shutdown gracefully."},
        KV{.key = "cache.num_diffs", .value = "Number of diffs to cache. For more info, consult readme.md in etc"},
        KV{.key = "cache.num_markers", .value = "Number of markers to cache."},
        KV{.key = "cache.num_cursors_from_diff", .value = "Num of cursors that are different."},
        KV{.key = "cache.num_cursors_from_account", .value = "Number of cursors from an account."},
        KV{.key = "cache.page_fetch_size", .value = "Page fetch size for cache operations."},
        KV{.key = "cache.load", .value = "Cache loading strategy ('sync' or 'async')."},
        KV{.key = "log_channels.[].channel",
           .value = "Name of the log channel."
                    "'RPC', 'ETL', and 'Performance'"},
        KV{.key = "log_channels.[].log_level",
           .value = "Log level for the specific log channel."
                    "`warning`, `error`, `fatal`"},
        KV{.key = "log_level",
           .value = "General logging level of Clio. This level will be applied to all log channels that do not have an "
                    "explicitly defined logging level."},
        KV{.key = "log_format", .value = "Format string for log messages."},
        KV{.key = "log_to_console", .value = "Enable or disable logging to console."},
        KV{.key = "log_directory", .value = "Directory path for log files."},
        KV{.key = "log_rotation_size",
           .value =
               "Log rotation size in megabytes. When the log file reaches this particular size, a new log file starts."
        },
        KV{.key = "log_directory_max_size", .value = "Maximum size of the log directory in megabytes."},
        KV{.key = "log_rotation_hour_interval",
           .value = "Interval in hours for log rotation. If the current log file reaches this value in logging, a new "
                    "log file starts."},
        KV{.key = "log_tag_style", .value = "Style for log tags."},
        KV{.key = "extractor_threads", .value = "Number of extractor threads."},
        KV{.key = "read_only", .value = "Indicates if the server should have read-only privileges."},
        KV{.key = "txn_threshold", .value = "Transaction threshold value."},
        KV{.key = "start_sequence", .value = "Starting ledger index."},
        KV{.key = "finish_sequence", .value = "Ending ledger index."},
        KV{.key = "ssl_cert_file", .value = "Path to the SSL certificate file."},
        KV{.key = "ssl_key_file", .value = "Path to the SSL key file."},
        KV{.key = "api_version.default", .value = "Default API version Clio will run on."},
        KV{.key = "api_version.min", .value = "Minimum API version."},
        KV{.key = "api_version.max", .value = "Maximum API version."},
        KV{.key = "migration.full_scan_threads", .value = "The number of threads used to scan the table."},
        KV{.key = "migration.full_scan_jobs", .value = "The number of coroutines used to scan the table."},
        KV{.key = "migration.cursors_per_job", .value = "The number of cursors each coroutine will scan."}
    };
};

}  // namespace util::config
