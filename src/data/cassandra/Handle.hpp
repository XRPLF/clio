//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "data/cassandra/Error.hpp"
#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/Batch.hpp"
#include "data/cassandra/impl/Cluster.hpp"
#include "data/cassandra/impl/Future.hpp"
#include "data/cassandra/impl/ManagedObject.hpp"
#include "data/cassandra/impl/Result.hpp"
#include "data/cassandra/impl/Session.hpp"
#include "data/cassandra/impl/Statement.hpp"

#include <cassandra.h>

#include <functional>
#include <string_view>
#include <vector>

namespace data::cassandra {

/**
 * @brief Represents a handle to the cassandra database cluster
 */
class Handle {
    impl::Cluster cluster_;
    impl::Session session_;

public:
    using ResultOrErrorType = ResultOrError;
    using MaybeErrorType = MaybeError;
    using FutureWithCallbackType = FutureWithCallback;
    using FutureType = Future;
    using StatementType = Statement;
    using PreparedStatementType = PreparedStatement;
    using ResultType = Result;

    /**
     * @brief Construct a new handle from a @ref impl::Settings object.
     *
     * @param clusterSettings The settings to use
     */
    explicit Handle(Settings clusterSettings = Settings::defaultSettings());

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
     * @brief Connect to the cluster asynchronously.
     *
     * @return A future
     */
    [[nodiscard]] FutureType
    asyncConnect() const;

    /**
     * @brief Synchonous version of the above.
     *
     * See @ref asyncConnect() const for how this works.
     *
     * @return Possibly an error
     */
    [[nodiscard]] MaybeErrorType
    connect() const;

    /**
     * @brief Connect to the the specified keyspace asynchronously.
     *
     * @param keyspace The keyspace to use
     * @return A future
     */
    [[nodiscard]] FutureType
    asyncConnect(std::string_view keyspace) const;

    /**
     * @brief Synchonous version of the above.
     *
     * See @ref asyncConnect(std::string_view) const for how this works.
     *
     * @param keyspace The keyspace to use
     * @return Possibly an error
     */
    [[nodiscard]] MaybeErrorType
    connect(std::string_view keyspace) const;

    /**
     * @brief Disconnect from the cluster asynchronously.
     *
     * @return A future
     */
    [[nodiscard]] FutureType
    asyncDisconnect() const;

    /**
     * @brief Synchonous version of the above.
     *
     * See @ref asyncDisconnect() const for how this works.
     *
     * @return Possibly an error
     */
    [[maybe_unused]] MaybeErrorType
    disconnect() const;

    /**
     * @brief Reconnect to the the specified keyspace asynchronously.
     *
     * @param keyspace The keyspace to use
     * @return A future
     */
    [[nodiscard]] FutureType
    asyncReconnect(std::string_view keyspace) const;

    /**
     * @brief Synchonous version of the above.
     *
     * See @ref asyncReconnect(std::string_view) const for how this works.
     *
     * @param keyspace The keyspace to use
     * @return Possibly an error
     */
    [[nodiscard]] MaybeErrorType
    reconnect(std::string_view keyspace) const;

    /**
     * @brief Execute a simple query with optional args asynchronously.
     *
     * @param query The query to execute
     * @param args The arguments to bind for execution
     * @return A future
     */
    template <typename... Args>
    [[nodiscard]] FutureType
    asyncExecute(std::string_view query, Args&&... args) const
    {
        auto statement = StatementType{query, std::forward<Args>(args)...};
        return cass_session_execute(session_, statement);
    }

    /**
     * @brief Synchonous version of the above.
     *
     * See asyncExecute(std::string_view, Args&&...) const for how this works.
     *
     * @param query The query to execute
     * @param args The arguments to bind for execution
     * @return The result or an error
     */
    template <typename... Args>
    [[maybe_unused]] ResultOrErrorType
    execute(std::string_view query, Args&&... args) const
    {
        return asyncExecute<Args...>(query, std::forward<Args>(args)...).get();
    }

    /**
     * @brief Execute each of the statements asynchronously.
     *
     * Batched version is not always the right option.
     * Especially since it only supports INSERT, UPDATE and DELETE statements.
     * This can be used as an alternative when statements need to execute in bulk.
     *
     * @param statements The statements to execute
     * @return A vector of future objects
     */
    [[nodiscard]] std::vector<FutureType>
    asyncExecuteEach(std::vector<StatementType> const& statements) const;

    /**
     * @brief Synchonous version of the above.
     *
     * See @ref asyncExecuteEach(std::vector<StatementType> const&) const for how this works.
     *
     * @param statements The statements to execute
     * @return Possibly an error
     */
    [[maybe_unused]] MaybeErrorType
    executeEach(std::vector<StatementType> const& statements) const;

    /**
     * @brief Execute a prepared statement with optional args asynchronously.
     *
     * @param statement The prepared statement to execute
     * @param args The arguments to bind for execution
     * @return A future
     */
    template <typename... Args>
    [[nodiscard]] FutureType
    asyncExecute(PreparedStatementType const& statement, Args&&... args) const
    {
        auto bound = statement.bind<Args...>(std::forward<Args>(args)...);
        return cass_session_execute(session_, bound);
    }

    /**
     * @brief Synchonous version of the above.
     *
     * See asyncExecute(std::vector<StatementType> const&, Args&&...) const for how this works.
     *
     * @param statement The prepared statement to bind and execute
     * @param args The arguments to bind for execution
     * @return The result or an error
     */
    template <typename... Args>
    [[maybe_unused]] ResultOrErrorType
    execute(PreparedStatementType const& statement, Args&&... args) const
    {
        return asyncExecute<Args...>(statement, std::forward<Args>(args)...).get();
    }

    /**
     * @brief Execute one (bound or simple) statements asynchronously.
     *
     * @param statement The statement to execute
     * @return A future
     */
    [[nodiscard]] FutureType
    asyncExecute(StatementType const& statement) const;

    /**
     * @brief Execute one (bound or simple) statements asynchronously with a callback.
     *
     * @param statement The statement to execute
     * @param cb The callback to execute when data is ready
     * @return A future that holds onto the callback provided
     */
    [[nodiscard]] FutureWithCallbackType
    asyncExecute(StatementType const& statement, std::function<void(ResultOrErrorType)>&& cb) const;

    /**
     * @brief Synchonous version of the above.
     *
     * See @ref asyncExecute(StatementType const&) const for how this works.
     *
     * @param statement The statement to execute
     * @return The result or an error
     */
    [[maybe_unused]] ResultOrErrorType
    execute(StatementType const& statement) const;

    /**
     * @brief Execute a batch of (bound or simple) statements asynchronously.
     *
     * @param statements The statements to execute
     * @return A future
     */
    [[nodiscard]] FutureType
    asyncExecute(std::vector<StatementType> const& statements) const;

    /**
     * @brief Synchonous version of the above.
     *
     * See @ref asyncExecute(std::vector<StatementType> const&) const for how this works.
     *
     * @param statements The statements to execute
     * @return Possibly an error
     */
    [[maybe_unused]] MaybeErrorType
    execute(std::vector<StatementType> const& statements) const;

    /**
     * @brief Execute a batch of (bound or simple) statements asynchronously with a completion callback.
     *
     * @param statements The statements to execute
     * @param cb The callback to execute when data is ready
     * @return A future that holds onto the callback provided
     */
    [[nodiscard]] FutureWithCallbackType
    asyncExecute(std::vector<StatementType> const& statements, std::function<void(ResultOrErrorType)>&& cb) const;

    /**
     * @brief Prepare a statement.
     *
     * @param query
     * @return A prepared statement
     * @throws std::runtime_error with underlying error description on failure
     */
    [[nodiscard]] PreparedStatementType
    prepare(std::string_view query) const;
};

/**
 * @brief Extracts the results into series of std::tuple<Types...> by creating a simple wrapper with an STL input
 * iterator inside.
 *
 * You can call .begin() and .end() in order to iterate as usual.
 * This also means that you can use it in a range-based for or with some algorithms.
 *
 * @param result The result to iterate
 */
template <typename... Types>
[[nodiscard]] impl::ResultExtractor<Types...>
extract(Handle::ResultType const& result)
{
    return {result};
}

}  // namespace data::cassandra
