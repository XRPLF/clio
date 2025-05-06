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
        file << "# Clio Config Description\n\n";
        file << "This document provides a list of all available Clio configuration properties in detail.\n\n";
        file << "> [!NOTE]\n";
        file << "> Dot notation in configuration key names represents nested fields. For example, "
                "**database.scylladb** refers to the _scylladb_ field inside the _database_ object. If a key name "
                "includes \"[]\", it indicates that the nested field is an array (e.g., etl_sources.[]).\n\n";
        file << "## Configuration Details\n";

        for (auto const& [key, val] : kCONFIG_DESCRIPTION) {
            file << "\n### " << key << "\n\n";

            // Every type of value is directed to operator<< in ConfigValue.hpp
            // as ConfigValue is the one that holds all the info regarding the config values
            if (key.contains("[]")) {
                file << gClioConfig.asArray(key);
            } else {
                file << gClioConfig.getValueView(key);
            }
            file << "- **Description**: " << val << "\n";
        }
    }

private:
    static constexpr auto kCONFIG_DESCRIPTION = std::array{
        KV{.key = "database.type",
           .value =
               "Specifies the type of database used for storing and retrieving data required by the Clio server. Both "
               "ScyllaDB and Cassandra can serve as backends for Clio; however, this value must be set to `cassandra`."
        },
        KV{.key = "database.cassandra.contact_points",
           .value = "A list of IP addresses or hostnames for the initial cluster nodes (Cassandra or ScyllaDB) that "
                    "the client connects to when establishing a database connection. If you're running Clio locally, "
                    "set this value to `localhost` or `127.0.0.1`."},
        KV{.key = "database.cassandra.secure_connect_bundle",
           .value = "The configuration file that contains the necessary credentials and connection details for "
                    "securely connecting to a Cassandra database cluster."},
        KV{.key = "database.cassandra.port", .value = "The port number used to connect to the Cassandra database."},
        KV{.key = "database.cassandra.keyspace",
           .value = "The Cassandra keyspace to use for the database. If you don't provide a value, this is set to "
                    "`clio` by default."},
        KV{.key = "database.cassandra.replication_factor",
           .value = "Represents the number of replicated nodes for ScyllaDB. For more details see [Fault Tolerance "
                    "Replication "
                    "Factor](https://university.scylladb.com/courses/scylla-essentials-overview/lessons/"
                    "high-availability/topic/fault-tolerance-replication-factor/)."},
        KV{.key = "database.cassandra.table_prefix",
           .value = "An optional field to specify a prefix for the Cassandra database table names."},
        KV{.key = "database.cassandra.max_write_requests_outstanding",
           .value = "Represents the maximum number of outstanding write requests. Write requests are API calls that "
                    "write to the database."},
        KV{.key = "database.cassandra.max_read_requests_outstanding",
           .value =
               "Maximum number of outstanding read requests. Read requests are API calls that read from the database."},
        KV{.key = "database.cassandra.threads",
           .value = "Represents the number of threads that will be used for database operations."},
        KV{.key = "database.cassandra.core_connections_per_host",
           .value = "The number of core connections per host for the Cassandra database."},
        KV{.key = "database.cassandra.queue_size_io",
           .value = "Defines the queue size of the input/output (I/O) operations in Cassandra."},
        KV{.key = "database.cassandra.write_batch_size",
           .value = "Represents the batch size for write operations in Cassandra."},
        KV{.key = "database.cassandra.connect_timeout",
           .value = "The maximum amount of time in seconds that the system waits for a database connection to be "
                    "established."},
        KV{.key = "database.cassandra.request_timeout",
           .value = "The maximum amount of time in seconds that the system waits for a request to be fetched from the "
                    "database."},
        KV{.key = "database.cassandra.username", .value = "The username used for authenticating with the database."},
        KV{.key = "database.cassandra.password", .value = "The password used for authenticating with the database."},
        KV{.key = "database.cassandra.certfile",
           .value = "The path to the SSL/TLS certificate file used to establish a secure connection between the client "
                    "and the Cassandra database."},
        KV{.key = "allow_no_etl", .value = "If set to `True`, allows Clio to start without any ETL source."},
        KV{.key = "etl_sources.[].ip", .value = "The IP address of the ETL source."},
        KV{.key = "etl_sources.[].ws_port", .value = "The WebSocket port of the ETL source."},
        KV{.key = "etl_sources.[].grpc_port", .value = "The gRPC port of the ETL source."},
        KV{.key = "forwarding.cache_timeout",
           .value = "Specifies the timeout duration (in seconds) for the forwarding cache used in `rippled` "
                    "communication. A value of `0` means disabling this feature."},
        KV{.key = "forwarding.request_timeout",
           .value =
               "Specifies the timeout duration (in seconds) for the forwarding request used in `rippled` communication."
        },
        KV{.key = "rpc.cache_timeout",
           .value = "Specifies the timeout duration (in seconds) for RPC cache response to timeout. A value of `0` "
                    "means disabling this feature."},
        KV{.key = "num_markers", .value = "Specifies the number of coroutines used to download the initial ledger."},
        KV{.key = "dos_guard.whitelist.[]", .value = "The list of IP addresses to whitelist for DOS protection."},
        KV{.key = "dos_guard.max_fetches", .value = "The maximum number of fetch operations allowed by DOS guard."},
        KV{.key = "dos_guard.max_connections",
           .value = "The maximum number of concurrent connections for a specific IP address."},
        KV{.key = "dos_guard.max_requests", .value = "The maximum number of requests allowed for a specific IP address."
        },
        KV{.key = "dos_guard.sweep_interval", .value = "Interval in seconds for DOS guard to sweep(clear) its state."},
        KV{.key = "workers", .value = "The number of threads used to process RPC requests."},
        KV{.key = "server.ip", .value = "The IP address of the Clio HTTP server."},
        KV{.key = "server.port", .value = "The port number of the Clio HTTP server."},
        KV{.key = "server.max_queue_size",
           .value =
               "The maximum size of the server's request queue. If set to `0`, this means there is no queue size limit."
        },
        KV{.key = "server.local_admin",
           .value = "Indicates if requests from `localhost` are allowed to call Clio admin-only APIs. Note that this "
                    "setting cannot be enabled "
                    "together with [server.admin_password](#serveradmin_password)."},
        KV{.key = "server.admin_password",
           .value = "The password for Clio admin-only APIs. Note that this setting cannot be enabled together with "
                    "[server.local_admin](#serveradmin_password)."},
        KV{.key = "server.processing_policy",
           .value = "For the `sequent` policy, requests from a single client connection are processed one by one, with "
                    "the next request read only after the previous one is processed. For the `parallel` policy, Clio "
                    "will accept all requests and process them in parallel, sending a reply for each request as soon "
                    "as it is ready."},
        KV{.key = "server.parallel_requests_limit",
           .value = "This is an optional parameter, used only if the `processing_strategy` is `parallel`. It limits "
                    "the number of requests processed in parallel for a single client connection. If not specified, no "
                    "limit is enforced."},
        KV{.key = "server.ws_max_sending_queue_size",
           .value = "Maximum queue size for sending subscription data to clients. This queue buffers data when a "
                    "client is slow to receive it, ensuring delivery once the client is ready."},
        KV{.key = "prometheus.enabled", .value = "Enables or disables Prometheus metrics."},
        KV{.key = "prometheus.compress_reply", .value = "Enables or disables compression of Prometheus responses."},
        KV{.key = "io_threads", .value = "The number of input/output (I/O) threads. The value cannot be less than `1`."
        },
        KV{.key = "subscription_workers",
           .value = "The number of worker threads or processes that are responsible for managing and processing "
                    "subscription-based tasks from `rippled`."},
        KV{.key = "graceful_period",
           .value = "The number of milliseconds the server waits to shutdown gracefully. If Clio does not shutdown "
                    "gracefully after the specified value, it will be killed instead."},
        KV{.key = "cache.num_diffs",
           .value = "The number of cursors generated is the number of changed (without counting deleted) objects in "
                    "the latest `cache.num_diffs` number of ledgers. Cursors are workers that load the ledger cache "
                    "from the position of markers concurrently. For more information, please read "
                    "[README.md](../src/etl/README.md)."},
        KV{.key = "cache.num_markers",
           .value = "Specifies how many markers are placed randomly within the cache. These markers define the "
                    "positions on the ledger that will be loaded concurrently by the workers. The higher the number, "
                    "the more places within the cache we potentially cover."},
        KV{.key = "cache.num_cursors_from_diff",
           .value = "`cache.num_cursors_from_diff` number of cursors are generated by looking at the number of changed "
                    "objects in the most recent ledger. If number of changed objects in current ledger is not enough, "
                    "it will keep reading previous ledgers until it hit `cache.num_cursors_from_diff`. If set to `0`, "
                    "the system defaults to generating cursors based on `cache.num_diffs`."},
        KV{.key = "cache.num_cursors_from_account",
           .value = "`cache.num_cursors_from_diff` of cursors are generated by reading accounts in `account_tx` table. "
                    "If set to `0`, the system defaults to generating cursors based on `cache.num_diffs`."},
        KV{.key = "cache.page_fetch_size", .value = "The number of ledger objects to fetch concurrently per marker."},
        KV{.key = "cache.load", .value = "The strategy used for Cache loading."},
        KV{.key = "log_channels.[].channel", .value = "The name of the log channel."},
        KV{.key = "log_channels.[].log_level", .value = "The log level for the specific log channel."},
        KV{.key = "log_level",
           .value = "The general logging level of Clio. This level is applied to all log channels that do not have an "
                    "explicitly defined logging level."},
        KV{.key = "log_format",
           .value = "The format string for log messages. The format is described here: "
                    "<https://www.boost.org/doc/libs/1_83_0/libs/log/doc/html/log/tutorial/formatters.html>."},
        KV{.key = "log_to_console", .value = "Enables or disables logging to the console."},
        KV{.key = "log_directory", .value = "The directory path for the log files."},
        KV{.key = "log_rotation_size",
           .value = "The log rotation size in megabytes. When the log file reaches this particular size, a new log "
                    "file starts."},
        KV{.key = "log_directory_max_size", .value = "The maximum size of the log directory in megabytes."},
        KV{.key = "log_rotation_hour_interval",
           .value = "Represents the interval (in hours) for log rotation. If the current log file reaches this value "
                    "in logging, a new log file starts."},
        KV{.key = "log_tag_style",
           .value =
               "Log tags are unique identifiers for log messages. `uint`/`int` starts logging from 0 and increments, "
               "making it faster. In contrast, `uuid` generates a random unique identifier, which adds overhead."},
        KV{.key = "extractor_threads", .value = "Number of threads used to extract data from ETL source."},
        KV{.key = "read_only", .value = "Indicates if the server is allowed to write data to the database."},
        KV{.key = "start_sequence",
           .value = "If specified, the ledger index Clio will start writing to the database from."},
        KV{.key = "finish_sequence", .value = "If specified, the final ledger that Clio will write to the database."},
        KV{.key = "ssl_cert_file", .value = "The path to the SSL certificate file."},
        KV{.key = "ssl_key_file", .value = "The path to the SSL key file."},
        KV{.key = "api_version.default", .value = "The default API version that the Clio server will run on."},
        KV{.key = "api_version.min", .value = "The minimum API version allowed to use."},
        KV{.key = "api_version.max", .value = "The maximum API version allowed to use."},
        KV{.key = "migration.full_scan_threads", .value = "The number of threads used to scan the table."},
        KV{.key = "migration.full_scan_jobs", .value = "The number of coroutines used to scan the table."},
        KV{.key = "migration.cursors_per_job", .value = "The number of cursors each job will scan."}
    };
};

}  // namespace util::config
