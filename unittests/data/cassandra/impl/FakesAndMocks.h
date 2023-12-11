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

#include "data/cassandra/Error.h"
#include "data/cassandra/impl/AsyncExecutor.h"

#include <gmock/gmock.h>

#include <vector>

using namespace data::cassandra;
using namespace data::cassandra::detail;

struct FakeResult {};

struct FakeResultOrError {
    CassandraError err{"<default>", CASS_OK};

    operator bool() const
    {
        return err.code() == CASS_OK;
    }

    CassandraError
    error() const
    {
        return err;
    }

    static FakeResult
    value() 
    {
        return FakeResult{};
    }
};

struct FakeMaybeError {};

struct FakeStatement {};

struct FakePreparedStatement {};

struct FakeFuture {
    FakeResultOrError data;

    FakeResultOrError
    get() const
    {
        return data;
    }

    static FakeMaybeError
    await() 
    {
        return {};
    }
};

struct FakeFutureWithCallback : public FakeFuture {};

struct MockHandle {
    using ResultOrErrorType = FakeResultOrError;
    using MaybeErrorType = FakeMaybeError;
    using FutureWithCallbackType = FakeFutureWithCallback;
    using FutureType = FakeFuture;
    using StatementType = FakeStatement;
    using PreparedStatementType = FakePreparedStatement;
    using ResultType = FakeResult;

    MOCK_METHOD(
        FutureWithCallbackType,
        asyncExecute,
        (StatementType const&, std::function<void(ResultOrErrorType)>&&),
        (const)
    );

    MOCK_METHOD(
        FutureWithCallbackType,
        asyncExecute,
        (std::vector<StatementType> const&, std::function<void(ResultOrErrorType)>&&),
        (const)
    );

    MOCK_METHOD(ResultOrErrorType, execute, (StatementType const&), (const));
};

struct FakeRetryPolicy {
    FakeRetryPolicy(boost::asio::io_context&){};  // required by concept

    static std::chrono::milliseconds
    calculateDelay(uint32_t /* attempt */)
    {
        return std::chrono::milliseconds{1};
    }

    static bool
    shouldRetry(CassandraError) 
    {
        return false;
    }

    template <typename Fn>
    void
    retry(Fn&& fn)
    {
        fn();
    }
};
