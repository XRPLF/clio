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
#include "data/clickhouse/impl/Connection.hpp"
#include "util/log/Logger.hpp"

#include <memory>
#include <string>
#include <vector>

namespace data::clickhouse {

/**
 * @brief A handle to a ClickHouse database connection.
 *
 * This class provides a simplified interface for database operations,
 * managing the underlying connection and providing error handling.
 */
class Handle {
    util::Logger log_{"ClickHouseHandle"};
    std::unique_ptr<impl::Connection> connection_;

public:
    /**
     * @brief Construct a new Handle object.
     *
     * @param settings The connection settings to use
     */
    explicit Handle(Settings const& settings);

    /**
     * @brief Destructor.
     */
    ~Handle();

    /**
     * @brief Move constructor.
     */
    Handle(Handle&&) noexcept;

    /**
     * @brief Move assignment operator.
     */
    Handle& operator=(Handle&&) noexcept;

    /**
     * @brief Deleted copy constructor.
     */
    Handle(Handle const&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    Handle& operator=(Handle const&) = delete;

    /**
     * @brief Connect to the ClickHouse database.
     *
     * @return MaybeError indicating success or failure
     */
    MaybeError connect();

    /**
     * @brief Execute a query.
     *
     * @param query The SQL query to execute
     * @return MaybeError indicating success or failure
     */
    MaybeError execute(std::string const& query);

    /**
     * @brief Execute multiple queries.
     *
     * @param queries The SQL queries to execute
     * @return MaybeError indicating success or failure
     */
    MaybeError executeEach(std::vector<std::string> const& queries);

    /**
     * @brief Execute a query and return results.
     *
     * @param query The SQL query to execute
     * @return ResultOrError containing the results or an error
     */
    ResultOrError query(std::string const& query);

    /**
     * @brief Check if the connection is active.
     *
     * @return true if connected, false otherwise
     */
    bool isConnected() const;

private:
    /**
     * @brief Initialize the connection.
     *
     * @param settings The connection settings
     */
    void initialize(Settings const& settings);
};

}  // namespace data::clickhouse
