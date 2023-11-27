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

#include "data/cassandra/Handle.h"
#include "data/cassandra/Types.h"
#include <cassandra.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

std::vector<Handle::FutureType>
Handle::asyncExecuteEach(std::vector<Statement> const& statements) const
{
    std::vector<Handle::FutureType> futures;
    futures.reserve(statements.size());
    for (auto const& statement : statements)
        futures.emplace_back(cass_session_execute(session_, statement));
    return futures;
}

Handle::MaybeErrorType
Handle::executeEach(std::vector<Statement> const& statements) const
{
    for (auto futures = asyncExecuteEach(statements); auto const& future : futures) {
        if (auto rc = future.await(); not rc)
            return rc;
    }

    return {};
}

Handle::FutureType
Handle::asyncExecute(Statement const& statement) const
{
    return cass_session_execute(session_, statement);
}

Handle::FutureWithCallbackType
Handle::asyncExecute(Statement const& statement, std::function<void(Handle::ResultOrErrorType)>&& cb) const
{
    return Handle::FutureWithCallbackType{cass_session_execute(session_, statement), std::move(cb)};
}

Handle::ResultOrErrorType
Handle::execute(Statement const& statement) const
{
    return asyncExecute(statement).get();
}

Handle::FutureType
Handle::asyncExecute(std::vector<Statement> const& statements) const
{
    return cass_session_execute_batch(session_, Batch{statements});
}

Handle::MaybeErrorType
Handle::execute(std::vector<Statement> const& statements) const
{
    return asyncExecute(statements).await();
}

Handle::FutureWithCallbackType
Handle::asyncExecute(std::vector<Statement> const& statements, std::function<void(Handle::ResultOrErrorType)>&& cb)
    const
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
