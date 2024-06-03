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

#include "data/cassandra/Types.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace data::cassandra {

/**
 * @brief The requirements of a settings provider.
 */
template <typename T>
concept SomeSettingsProvider = requires(T a) {
    { a.getSettings() } -> std::same_as<Settings>;
    { a.getKeyspace() } -> std::same_as<std::string>;
    { a.getTablePrefix() } -> std::same_as<std::optional<std::string>>;
    { a.getReplicationFactor() } -> std::same_as<uint16_t>;
    { a.getTtl() } -> std::same_as<uint16_t>;
};

/**
 * @brief The requirements of an execution strategy.
 */
template <typename T>
concept SomeExecutionStrategy = requires(
    T a,
    Settings settings,
    Handle handle,
    Statement statement,
    std::vector<Statement> statements,
    PreparedStatement prepared,
    boost::asio::yield_context token
) {
    { T(settings, handle) };
    { a.sync() } -> std::same_as<void>;
    { a.isTooBusy() } -> std::same_as<bool>;
    { a.writeSync(statement) } -> std::same_as<ResultOrError>;
    { a.writeSync(prepared) } -> std::same_as<ResultOrError>;
    { a.write(prepared) } -> std::same_as<void>;
    { a.write(std::move(statements)) } -> std::same_as<void>;
    { a.read(token, prepared) } -> std::same_as<ResultOrError>;
    { a.read(token, statement) } -> std::same_as<ResultOrError>;
    { a.read(token, statements) } -> std::same_as<ResultOrError>;
    { a.readEach(token, statements) } -> std::same_as<std::vector<Result>>;
    { a.stats() } -> std::same_as<boost::json::object>;
};

/**
 * @brief The requirements of a retry policy.
 */
template <typename T>
concept SomeRetryPolicy = requires(T a, boost::asio::io_context ioc, CassandraError err, uint32_t attempt) {
    { T(ioc) };
    { a.shouldRetry(err) } -> std::same_as<bool>;
    {
        a.retry([]() {})
    } -> std::same_as<void>;
    { a.calculateDelay(attempt) } -> std::same_as<std::chrono::milliseconds>;
};

}  // namespace data::cassandra
