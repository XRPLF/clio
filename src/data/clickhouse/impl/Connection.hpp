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

#include "data/clickhouse/Types.hpp"
#include "util/log/Logger.hpp"

#include <memory>
#include <string>
#include <optional>
#include <chrono>
#include <thread>

namespace data::clickhouse::impl {



/**
 * @brief Forward declaration of the ClickHouse connection implementation.
 * This will be implemented using the ClickHouse C++ client library.
 */
class ConnectionImpl;

/**
 * @brief Manages a ClickHouse connection.
 */
class Connection {
    util::Logger log_{"ClickHouse"};
    std::unique_ptr<ConnectionImpl> impl_;

public:
    /**
     * @brief Construct a new Connection object.
     *
     * @param settings The connection settings to use
     */
    explicit Connection(Settings const& settings);

    /**
     * @brief Destructor.
     */
    ~Connection();

    /**
     * @brief Move constructor.
     */
    Connection(Connection&&) noexcept;

    /**
     * @brief Move assignment operator.
     */
    Connection& operator=(Connection&&) noexcept;

    /**
     * @brief Deleted copy constructor.
     */
    Connection(Connection const&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    Connection& operator=(Connection const&) = delete;

    /**
     * @brief Connect to the ClickHouse server.
     *
     * @return true on success, false otherwise
     */
    bool connect();

    /**
     * @brief Check if the connection is active.
     *
     * @return true if connected, false otherwise
     */
    bool isConnected() const;

    /**
     * @brief Execute a query.
     *
     * @param query The SQL query to execute
     * @return true on success, false otherwise
     */
    bool execute(std::string const& query);

    /**
     * @brief Execute a query and return results.
     *
     * @param query The SQL query to execute
     * @return A result object containing the query results
     */
    std::unique_ptr<Result> query(std::string const& query);

    /**
     * @brief Prepare a statement.
     *
     * @param query The SQL query to prepare
     * @return A prepared statement object
     */
    std::unique_ptr<PreparedStatement> prepare(std::string const& query);

private:
    /**
     * @brief Initialize the connection implementation.
     *
     * @param settings The connection settings
     */
    void initialize(Settings const& settings);
};

}  // namespace data::clickhouse::impl
