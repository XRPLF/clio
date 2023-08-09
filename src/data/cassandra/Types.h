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

#include <util/Expected.h>

#include <string>

namespace data::cassandra {

namespace detail {
struct Settings;
class Session;
class Cluster;
struct Future;
class FutureWithCallback;
struct Result;
class Statement;
class PreparedStatement;
struct Batch;
}  // namespace detail

using Settings = detail::Settings;
using Future = detail::Future;
using FutureWithCallback = detail::FutureWithCallback;
using Result = detail::Result;
using Statement = detail::Statement;
using PreparedStatement = detail::PreparedStatement;
using Batch = detail::Batch;

/**
 * @brief A strong type wrapper for int32_t
 *
 * This is unfortunately needed right now to support uint32_t properly
 * because clio uses bigint (int64) everywhere except for when one need
 * to specify LIMIT, which needs an int32 :-/
 */
struct Limit
{
    int32_t limit;
};

class Handle;
class CassandraError;

using MaybeError = clio::util::Expected<void, CassandraError>;
using ResultOrError = clio::util::Expected<Result, CassandraError>;
using Error = clio::util::Unexpected<CassandraError>;

}  // namespace data::cassandra
