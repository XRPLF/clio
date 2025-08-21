//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <utility>
#include <vector>
#include <chrono>
#include <thread>

namespace data::clickhouse {

namespace impl {
class Connection;
class Statement;
struct Batch;
}  // namespace impl

using Connection = impl::Connection;
using Statement = impl::Statement;
using Batch = impl::Batch;

/**
 * @brief A simple prepared statement class for ClickHouse.
 */
class PreparedStatement {
public:
    PreparedStatement() = default;
    ~PreparedStatement() = default;
    
    // Add methods as needed for the placeholder implementation
};

/**
 * @brief Bundles all ClickHouse settings in one place.
 */
struct Settings {
    static constexpr std::size_t kDEFAULT_CONNECTION_TIMEOUT = 10000;
    static constexpr uint32_t kDEFAULT_MAX_WRITE_REQUESTS_OUTSTANDING = 10'000;
    static constexpr uint32_t kDEFAULT_MAX_READ_REQUESTS_OUTSTANDING = 100'000;
    static constexpr std::size_t kDEFAULT_BATCH_SIZE = 20;

    /**
     * @brief Represents the configuration of contact points for ClickHouse.
     */
    struct ContactPoints {
        std::string host = "127.0.0.1";  // defaults to localhost
        uint16_t port = 9000;  // default ClickHouse port
        std::string database = "default";  // default database
    };

    /** @brief Enables or disables ClickHouse driver logger */
    bool enableLog = false;

    /** @brief Connect timeout specified in milliseconds */
    std::chrono::milliseconds connectionTimeout = std::chrono::milliseconds{kDEFAULT_CONNECTION_TIMEOUT};

    /** @brief Request timeout specified in milliseconds */
    std::chrono::milliseconds requestTimeout = std::chrono::milliseconds{0};  // no timeout at all

    /** @brief Connection information */
    ContactPoints connectionInfo = ContactPoints{};

    /** @brief The number of threads for the driver to pool */
    uint32_t threads = std::thread::hardware_concurrency();

    /** @brief The maximum number of outstanding write requests at any given moment */
    uint32_t maxWriteRequestsOutstanding = kDEFAULT_MAX_WRITE_REQUESTS_OUTSTANDING;

    /** @brief The maximum number of outstanding read requests at any given moment */
    uint32_t maxReadRequestsOutstanding = kDEFAULT_MAX_READ_REQUESTS_OUTSTANDING;

    /** @brief The number of connection per host to always have active */
    uint32_t coreConnectionsPerHost = 1u;

    /** @brief Size of batches when writing */
    std::size_t writeBatchSize = kDEFAULT_BATCH_SIZE;

    /** @brief Size of the IO queue */
    std::optional<uint32_t> queueSizeIO = std::nullopt;

    /** @brief Username/login */
    std::optional<std::string> username = std::nullopt;

    /** @brief Password to match the `username` */
    std::optional<std::string> password = std::nullopt;

    /**
     * @brief Creates a new Settings object as a copy of the current one with overridden contact points.
     */
    Settings
    withContactPoints(std::string_view host, uint16_t port = 9000, std::string_view database = "default")
    {
        auto tmp = *this;
        tmp.connectionInfo = ContactPoints{.host = std::string{host}, .port = port, .database = std::string{database}};
        return tmp;
    }

    /**
     * @brief Returns the default settings.
     */
    static Settings
    defaultSettings()
    {
        return Settings();
    }
};

/**
 * @brief A strong type wrapper for int32_t
 *
 * This is unfortunately needed right now to support uint32_t properly
 * because clio uses bigint (int64) everywhere except for when one need
 * to specify LIMIT, which needs an int32 :-/
 */
struct Limit {
    int32_t limit;
};

/**
 * @brief A strong type wrapper for string
 *
 * This is unfortunately needed right now to support TEXT properly
 * because clio uses string to represent BLOB
 * If we want to bind TEXT with string, we need to use this type
 */
struct Text {
    std::string text;

    /**
     * @brief Construct a new Text object from string type
     *
     * @param text The text to wrap
     */
    explicit Text(std::string text) : text{std::move(text)}
    {
    }
};

class Handle;
class ClickHouseError;

/**
 * @brief Represents a ClickHouse-specific error.
 */
class ClickHouseError {
    std::string message_;

public:
    explicit ClickHouseError(std::string message) : message_{std::move(message)}
    {
    }

    /**
     * @return The error message
     */
    std::string const&
    message() const
    {
        return message_;
    }

    /**
     * @return The error message as a C string
     */
    char const*
    what() const
    {
        return message_.c_str();
    }
};

/**
 * @brief Represents a ClickHouse query result.
 */
struct Result {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> columnNames;
    
    /**
     * @brief Get the number of rows in the result
     */
    size_t rowCount() const { return rows.size(); }
    
    /**
     * @brief Get the number of columns in the result
     */
    size_t columnCount() const { return columnNames.size(); }
    
    /**
     * @brief Check if the result is empty
     */
    bool empty() const { return rows.empty(); }
};

using MaybeError = std::expected<void, ClickHouseError>;
using ResultOrError = std::expected<Result, ClickHouseError>;
using Error = std::unexpected<ClickHouseError>;

}  // namespace data::clickhouse
