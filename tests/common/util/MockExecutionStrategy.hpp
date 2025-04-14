//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "data/cassandra/FakesAndMocks.hpp"
#include "data/cassandra/Handle.hpp"
#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/ExecutionStrategy.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>

#include <vector>

struct MockExecutionStrategy {
    MockExecutionStrategy([[maybe_unused]] auto&&, [[maybe_unused]] auto&&)
    {
    }

    using ResultOrErrorType = data::cassandra::Handle::ResultOrErrorType;
    using CompletionTokenType = boost::asio::yield_context;
    using StatementType = data::cassandra::Handle::StatementType;
    using PreparedStatementType = data::cassandra::Handle::PreparedStatementType;
    using ResultType = data::cassandra::Handle::ResultType;

    MOCK_METHOD(void, sync, (), ());
    MOCK_METHOD(bool, isTooBusy, (), (const));
    MOCK_METHOD(ResultOrError, writeSync, (StatementType const&), ());

    template <typename... Args>
    ResultOrErrorType
    writeSync(PreparedStatementType const& preparedStatement, Args&&... args)
    {
        return writeSync(preparedStatement.bind(std::forward<Args>(args)...));
    }

    MOCK_METHOD(void, write, (StatementType const&), ());

    template <typename... Args>
    void
    write(PreparedStatementType const& preparedStatement, Args&&... args)
    {
        auto statement = preparedStatement.bind(std::forward<Args>(args)...);
        write(std::move(statement));
    }

    MOCK_METHOD(void, write, (std::vector<StatementType>&&), ());

    MOCK_METHOD(ResultOrError, read, (boost::asio::yield_context, StatementType const&), ());
    MOCK_METHOD(ResultOrError, read, (boost::asio::yield_context, std::vector<StatementType> const&), ());

    template <typename... Args>
    ResultOrErrorType
    read(CompletionTokenType token, PreparedStatementType const& preparedStatement, Args&&... args)
    {
        return read(token, preparedStatement.bind(std::forward<Args>(args)...));
    }

    MOCK_METHOD(std::vector<Result>, readEach, (boost::asio::yield_context, std::vector<StatementType> const&), ());

    MOCK_METHOD(boost::json::object, stats, (), (const));

    MOCK_METHOD(void, writeEach, (std::vector<StatementType>&&), ());
};
