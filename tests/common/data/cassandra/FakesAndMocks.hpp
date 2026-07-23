#pragma once

#include "data/cassandra/Error.hpp"
#include "data/cassandra/impl/AsyncExecutor.hpp"

#include <boost/asio/io_context.hpp>
#include <cassandra.h>
#include <gmock/gmock.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

using namespace data::cassandra;
using namespace data::cassandra::impl;

struct FakeResult {};

struct FakeResultOrError {
    CassandraError err{"<default>", CASS_OK};

    operator bool() const
    {
        return err.code() == CASS_OK;
    }

    [[nodiscard]] CassandraError
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

    [[nodiscard]] FakeResultOrError
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
    FakeRetryPolicy(boost::asio::io_context&) {};  // required by concept

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
        std::invoke(std::forward<decltype(fn)>(fn));
    }
};
