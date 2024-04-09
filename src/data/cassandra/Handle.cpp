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

#include "data/cassandra/Handle.hpp"

#include "data/cassandra/Types.hpp"

#include <cassandra.h>

#include <stdexcept>
#include <string_view>
#include <utility>

namespace data::cassandra {

Handle::Handle(Settings clusterSettings) : cluster_{clusterSettings}
{
}

Handle::Handle(std::string_view contactPoints) : Handle{Settings::defaultSettings().withContactPoints(contactPoints)}
{
}

Handle::~Handle()
{
    [[maybe_unused]] auto _ = disconnect();  // attempt to disconnect
}

Handle::FutureType
Handle::asyncConnect() const
{
    return cass_session_connect(session_, cluster_);
}

Handle::MaybeErrorType
Handle::connect() const
{
    return asyncConnect().await();
}

Handle::FutureType
Handle::asyncConnect(std::string_view keyspace) const
{
    return cass_session_connect_keyspace(session_, cluster_, keyspace.data());
}

Handle::MaybeErrorType
Handle::connect(std::string_view keyspace) const
{
    return asyncConnect(keyspace).await();
}

Handle::FutureType
Handle::asyncDisconnect() const
{
    return cass_session_close(session_);
}

Handle::MaybeErrorType
Handle::disconnect() const
{
    return asyncDisconnect().await();
}

Handle::FutureType
Handle::asyncReconnect(std::string_view keyspace) const
{
    if (auto rc = asyncDisconnect().await(); not rc)  // sync
        throw std::logic_error("Reconnect to keyspace '" + std::string{keyspace} + "' failed: " + rc.error());
    return asyncConnect(keyspace);
}

Handle::MaybeErrorType
Handle::reconnect(std::string_view keyspace) const
{
    return asyncReconnect(keyspace).await();
}

static std::vector<Handle::FutureType>
Handle::asyncExecuteEach(std::vector<StatementType> const& statements)
{
    std::vector<Handle::FutureType> futures;
    futures.reserve(statements.size());
    for (auto const& statement : statements)
        futures.emplace_back(cass_session_execute(session_, statement));
    return futures;
}

static Handle::MaybeErrorType
Handle::executeEach(std::vector<StatementType> const& statements)
{
    for (auto futures = asyncExecuteEach(statements); auto const& future : futures) {
        if (auto rc = future.await(); not rc)
            return rc;
    }

    return {};
}

Handle::FutureType
Handle::asyncExecute(StatementType const& statement) const
{
    return cass_session_execute(session_, statement);
}

Handle::FutureWithCallbackType
Handle::asyncExecute(StatementType const& statement, std::function<void(ResultOrErrorType)>&& cb) const
{
    return Handle::FutureWithCallbackType{cass_session_execute(session_, statement), std::move(cb)};
}

Handle::ResultOrErrorType
Handle::execute(StatementType const& statement) const
{
    return asyncExecute(statement).get();
}

Handle::FutureType
Handle::asyncExecute(std::vector<StatementType> const& statements) const
{
    return cass_session_execute_batch(session_, Batch{statements});
}

static Handle::MaybeErrorType
Handle::execute(std::vector<StatementType> const& statements)
{
    return asyncExecute(statements).await();
}

Handle::FutureWithCallbackType
Handle::asyncExecute(std::vector<StatementType> const& statements, std::function<void(ResultOrErrorType)>&& cb) const
{
    return Handle::FutureWithCallbackType{cass_session_execute_batch(session_, Batch{statements}), std::move(cb)};
}

Handle::PreparedStatementType
Handle::prepare(std::string_view query) const
{
    Handle::FutureType const future = cass_session_prepare(session_, query.data());
    auto const rc = future.await();
    if (rc)
        return cass_future_get_prepared(future);

    throw std::runtime_error(rc.error().message());
}

}  // namespace data::cassandra
