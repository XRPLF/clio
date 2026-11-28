#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <utility>

namespace data::cassandra {
namespace impl {

struct Settings;
class Session;
class Cluster;
struct Future;
class FutureWithCallback;
struct Result;
class Statement;
class PreparedStatement;
struct Batch;

}  // namespace impl

using Settings = impl::Settings;                      ///< All Cassandra backend settings
using Future = impl::Future;                          ///< Future of an asynchronous query
using FutureWithCallback = impl::FutureWithCallback;  ///< Future that invokes a callback
using Result = impl::Result;                          ///< Result of a query
using Statement = impl::Statement;                    ///< A simple or bound statement
using PreparedStatement = impl::PreparedStatement;    ///< A prepared statement
using Batch = impl::Batch;                            ///< A batch of statements

/**
 * @brief A strong type wrapper for int32_t
 *
 * This is unfortunately needed right now to support uint32_t properly
 * because clio uses bigint (int64) everywhere except for when one need
 * to specify LIMIT, which needs an int32 :-/
 */
struct Limit {
    int32_t limit;  ///< The LIMIT value to bind
};

/**
 * @brief A strong type wrapper for string
 *
 * This is unfortunately needed right now to support TEXT properly
 * because clio uses string to represent BLOB
 * If we want to bind TEXT with string, we need to use this type
 */
struct Text {
    std::string text;  ///< The string to bind as TEXT

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
class CassandraError;

using MaybeError = std::expected<void, CassandraError>;       ///< Success or a database error
using ResultOrError = std::expected<Result, CassandraError>;  ///< A query result or an error
using Error = std::unexpected<CassandraError>;                ///< A database error

}  // namespace data::cassandra
