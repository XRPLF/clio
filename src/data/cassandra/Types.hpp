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

#include <cstdint>
#include <expected>

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

using Settings = impl::Settings;
using Future = impl::Future;
using FutureWithCallback = impl::FutureWithCallback;
using Result = impl::Result;
using Statement = impl::Statement;
using PreparedStatement = impl::PreparedStatement;
using Batch = impl::Batch;

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

class Handle;
class CassandraError;

using MaybeError = std::expected<void, CassandraError>;
using ResultOrError = std::expected<Result, CassandraError>;
using Error = std::unexpected<CassandraError>;

}  // namespace data::cassandra
