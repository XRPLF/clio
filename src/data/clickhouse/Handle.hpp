//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE  IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT,  OR  CONSEQUENTIAL  DAMAGES  OR  ANY
    DAMAGES  WHATSOEVER  RESULTING  FROM  LOSS  OF  USE,  DATA  OR  PROFITS,
    WHETHER  IN  AN  ACTION  OF  CONTRACT,  NEGLIGENCE  OR  OTHER  TORTIOUS
    ACTION,  ARISING  OUT  OF  OR  IN  CONNECTION  WITH  THE  USE  OR
    PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include "data/clickhouse/Error.hpp"
#include "data/clickhouse/Types.hpp"
#include "data/clickhouse/impl/Batch.hpp"
#include "data/clickhouse/impl/Cluster.hpp"
#include "data/clickhouse/impl/Result.hpp"
#include "data/clickhouse/impl/Session.hpp"
#include "data/clickhouse/impl/Statement.hpp"

#include <functional>
#include <string_view>
#include <vector>

/**
 * @brief This namespace implements a wrapper for the ClickHouse HTTP client
 */
namespace data::clickhouse {

/**
 * @brief Represents a handle to the ClickHouse database cluster
 */
class Handle {
    impl::Cluster cluster_;
    impl::Session session_;

public:
    using ResultOrErrorType = ResultOrError;
    using MaybeErrorType = MaybeError;
    using StatementType = Statement;
    using PreparedStatementType = PreparedStatement;
    using ResultType = Result;

    /**
     * @brief Construct a new handle from a Settings object.
     *
     * @param clusterSettings The settings to use
     */
    explicit Handle(Settings const& clusterSettings = Settings::defaultSettings());

    /**
     * @brief Construct a new handle with default settings and only by setting the contact points.
     *
     * @param contactPoints The contact points to use instead of settings
     */
    explicit Handle(std::string_view contactPoints);

    /**
     * @brief Disconnects gracefully if possible.
     */
    ~Handle();

    /**
     * @brief Move is supported.
     */
    Handle(Handle&&) = default;

    /**
     * @brief Connect to the cluster synchronously.
     *
     * @return Possibly an error
     */
    [[nodiscard]] MaybeErrorType
    connect() const;

    /**
     * @brief Execute a query without returning results.
     *
     * @param query The query to execute
     * @return Possibly an error
     */
    [[nodiscard]] MaybeErrorType
    execute(std::string const& query) const;

    /**
     * @brief Execute a batch of queries.
     *
     * @param batch The batch to execute
     * @return Possibly an error
     */
    [[nodiscard]] MaybeErrorType
    executeEach(std::vector<std::string> const& queries) const;

    /**
     * @brief Execute a query and return results.
     *
     * @param query The query to execute
     * @return A result or an error
     */
    [[nodiscard]] ResultOrErrorType
    query(std::string const& query) const;

    /**
     * @brief Check if the handle is connected.
     *
     * @return true if connected, false otherwise
     */
    [[nodiscard]] bool
    isConnected() const;

private:
    void
    initialize(Settings const& settings);

    void
    initialize(std::string_view contactPoints);
};

}  // namespace data::clickhouse
