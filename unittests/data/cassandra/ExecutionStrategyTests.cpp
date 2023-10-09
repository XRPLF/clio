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

#include <data/cassandra/impl/FakesAndMocks.h>
#include <util/Fixtures.h>

#include <data/cassandra/impl/ExecutionStrategy.h>

#include <gtest/gtest.h>

using namespace data::cassandra;
using namespace data::cassandra::detail;
using namespace testing;

class BackendCassandraExecutionStrategyTest : public SyncAsioContextTest
{
protected:
    class MockBackendCounters
    {
    public:
        using Ptr = std::shared_ptr<StrictMock<MockBackendCounters>>;
        static Ptr
        make()
        {
            return std::make_shared<StrictMock<MockBackendCounters>>();
        }

        MOCK_METHOD(void, registerTooBusy, (), ());
        MOCK_METHOD(void, registerWriteSync, (), ());
        MOCK_METHOD(void, registerWriteSyncRetry, (), ());
        MOCK_METHOD(void, registerWriteStarted, (), ());
        MOCK_METHOD(void, registerWriteFinished, (), ());
        MOCK_METHOD(void, registerWriteRetry, (), ());

        void
        registerReadStarted(std::uint64_t count = 1)
        {
            registerReadStartedImpl(count);
        }
        MOCK_METHOD(void, registerReadStartedImpl, (std::uint64_t), ());

        void
        registerReadFinished(std::uint64_t count = 1)
        {
            registerReadFinishedImpl(count);
        }
        MOCK_METHOD(void, registerReadFinishedImpl, (std::uint64_t), ());

        void
        registerReadRetry(std::uint64_t count = 1)
        {
            registerReadRetryImpl(count);
        }
        MOCK_METHOD(void, registerReadRetryImpl, (std::uint64_t), ());

        void
        registerReadError(std::uint64_t count = 1)
        {
            registerReadErrorImpl(count);
        }
        MOCK_METHOD(void, registerReadErrorImpl, (std::uint64_t), ());
        MOCK_METHOD(boost::json::object, report, (), ());
    };

    MockHandle handle_{};
    MockBackendCounters::Ptr counters_ = MockBackendCounters::make();
    static constexpr auto NUM_STATEMENTS = 3u;

    DefaultExecutionStrategy<MockHandle, MockBackendCounters>
    makeStrategy(Settings s = {})
    {
        return DefaultExecutionStrategy<MockHandle, MockBackendCounters>(s, handle_, counters_);
    }
};

TEST_F(BackendCassandraExecutionStrategyTest, IsTooBusy)
{
    {
        auto strat = makeStrategy(Settings{.maxReadRequestsOutstanding = 0});
        EXPECT_CALL(*counters_, registerTooBusy());
        EXPECT_TRUE(strat.isTooBusy());
    }
    auto strat = makeStrategy(Settings{.maxReadRequestsOutstanding = 1});
    EXPECT_FALSE(strat.isTooBusy());
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadOneInCoroutineSuccessful)
{
    auto strat = makeStrategy();

    ON_CALL(handle_, asyncExecute(A<FakeStatement const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const& /* statement */, auto&& cb) {
            cb({});  // pretend we got data
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(handle_, asyncExecute(A<FakeStatement const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);
    EXPECT_CALL(*counters_, registerReadStartedImpl(1));
    EXPECT_CALL(*counters_, registerReadFinishedImpl(1));

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statement = FakeStatement{};
        strat.read(yield, statement);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadOneInCoroutineThrowsOnTimeoutFailure)
{
    auto strat = makeStrategy();

    ON_CALL(handle_, asyncExecute(A<FakeStatement const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const&, auto&& cb) {
            auto res = FakeResultOrError{CassandraError{"timeout", CASS_ERROR_LIB_REQUEST_TIMED_OUT}};
            cb(res);  // notify that item is ready
            return FakeFutureWithCallback{res};
        });
    EXPECT_CALL(handle_, asyncExecute(A<FakeStatement const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);
    EXPECT_CALL(*counters_, registerReadStartedImpl(1));
    EXPECT_CALL(*counters_, registerReadErrorImpl(1));

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statement = FakeStatement{};
        EXPECT_THROW(strat.read(yield, statement), DatabaseTimeout);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadOneInCoroutineThrowsOnInvalidQueryFailure)
{
    auto strat = makeStrategy();

    ON_CALL(handle_, asyncExecute(A<FakeStatement const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const&, auto&& cb) {
            auto res = FakeResultOrError{CassandraError{"invalid", CASS_ERROR_SERVER_INVALID_QUERY}};
            cb(res);  // notify that item is ready
            return FakeFutureWithCallback{res};
        });
    EXPECT_CALL(handle_, asyncExecute(A<FakeStatement const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);
    EXPECT_CALL(*counters_, registerReadStartedImpl(1));
    EXPECT_CALL(*counters_, registerReadErrorImpl(1));

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statement = FakeStatement{};
        EXPECT_THROW(strat.read(yield, statement), std::runtime_error);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadBatchInCoroutineSuccessful)
{
    auto strat = makeStrategy();

    ON_CALL(
        handle_, asyncExecute(A<std::vector<FakeStatement> const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const& statements, auto&& cb) {
            EXPECT_EQ(statements.size(), NUM_STATEMENTS);
            cb({});  // pretend we got data
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(
        handle_, asyncExecute(A<std::vector<FakeStatement> const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);
    EXPECT_CALL(*counters_, registerReadStartedImpl(NUM_STATEMENTS));
    EXPECT_CALL(*counters_, registerReadFinishedImpl(NUM_STATEMENTS));

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statements = std::vector<FakeStatement>(NUM_STATEMENTS);
        strat.read(yield, statements);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadBatchInCoroutineThrowsOnTimeoutFailure)
{
    auto strat = makeStrategy();

    ON_CALL(
        handle_, asyncExecute(A<std::vector<FakeStatement> const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const& statements, auto&& cb) {
            EXPECT_EQ(statements.size(), NUM_STATEMENTS);
            auto res = FakeResultOrError{CassandraError{"timeout", CASS_ERROR_LIB_REQUEST_TIMED_OUT}};
            cb(res);  // notify that item is ready
            return FakeFutureWithCallback{res};
        });
    EXPECT_CALL(
        handle_, asyncExecute(A<std::vector<FakeStatement> const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);
    EXPECT_CALL(*counters_, registerReadStartedImpl(NUM_STATEMENTS));
    EXPECT_CALL(*counters_, registerReadErrorImpl(NUM_STATEMENTS));

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statements = std::vector<FakeStatement>(NUM_STATEMENTS);
        EXPECT_THROW(strat.read(yield, statements), DatabaseTimeout);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadBatchInCoroutineThrowsOnInvalidQueryFailure)
{
    auto strat = makeStrategy();

    ON_CALL(
        handle_, asyncExecute(A<std::vector<FakeStatement> const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const& statements, auto&& cb) {
            EXPECT_EQ(statements.size(), NUM_STATEMENTS);
            auto res = FakeResultOrError{CassandraError{"invalid", CASS_ERROR_SERVER_INVALID_QUERY}};
            cb(res);  // notify that item is ready
            return FakeFutureWithCallback{res};
        });
    EXPECT_CALL(
        handle_, asyncExecute(A<std::vector<FakeStatement> const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);
    EXPECT_CALL(*counters_, registerReadStartedImpl(NUM_STATEMENTS));
    EXPECT_CALL(*counters_, registerReadErrorImpl(NUM_STATEMENTS));

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statements = std::vector<FakeStatement>(NUM_STATEMENTS);
        EXPECT_THROW(strat.read(yield, statements), std::runtime_error);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadBatchInCoroutineMarksBusyIfRequestsOutstandingExceeded)
{
    auto strat = makeStrategy(Settings{.maxReadRequestsOutstanding = 2});

    ON_CALL(
        handle_, asyncExecute(A<std::vector<FakeStatement> const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([this, &strat](auto const& statements, auto&& cb) {
            EXPECT_EQ(statements.size(), NUM_STATEMENTS);
            EXPECT_CALL(*counters_, registerTooBusy());
            EXPECT_TRUE(strat.isTooBusy());  // 2 was the limit, we sent 3

            cb({});  // notify that item is ready
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(
        handle_, asyncExecute(A<std::vector<FakeStatement> const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);
    EXPECT_CALL(*counters_, registerReadStartedImpl(NUM_STATEMENTS));
    EXPECT_CALL(*counters_, registerReadFinishedImpl(NUM_STATEMENTS));

    runSpawn([&strat](boost::asio::yield_context yield) {
        EXPECT_FALSE(strat.isTooBusy());  // 2 was the limit, 0 atm
        auto statements = std::vector<FakeStatement>(NUM_STATEMENTS);
        strat.read(yield, statements);
        EXPECT_FALSE(strat.isTooBusy());  // after read completes it's 0 again
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadEachInCoroutineSuccessful)
{
    auto strat = makeStrategy();

    ON_CALL(handle_, asyncExecute(A<FakeStatement const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const&, auto&& cb) {
            cb({});  // pretend we got data
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(
        handle_,
        asyncExecute(
            A<FakeStatement const&>(),
            A<std::function<void(FakeResultOrError)>&&>()))
        .Times(NUM_STATEMENTS);  // once per statement
    EXPECT_CALL(*counters_, registerReadStartedImpl(NUM_STATEMENTS));
    EXPECT_CALL(*counters_, registerReadFinishedImpl(NUM_STATEMENTS));

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statements = std::vector<FakeStatement>(NUM_STATEMENTS);
        auto res = strat.readEach(yield, statements);
        EXPECT_EQ(res.size(), statements.size());
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadEachInCoroutineThrowsOnFailure)
{
    auto strat = makeStrategy();
    auto callCount = std::atomic_int{0};

    ON_CALL(handle_, asyncExecute(A<FakeStatement const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([&callCount](auto const&, auto&& cb) {
            if (callCount == 1)
            {  // error happens on one of the entries
                cb({CassandraError{"invalid data", CASS_ERROR_LIB_INVALID_DATA}});
            }
            else
            {
                cb({});  // pretend we got data
            }
            ++callCount;
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(
        handle_,
        asyncExecute(
            A<FakeStatement const&>(),
            A<std::function<void(FakeResultOrError)>&&>()))
        .Times(NUM_STATEMENTS);  // once per statement
    EXPECT_CALL(*counters_, registerReadStartedImpl(NUM_STATEMENTS));
    EXPECT_CALL(*counters_, registerReadErrorImpl(1));
    EXPECT_CALL(*counters_, registerReadFinishedImpl(2));

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statements = std::vector<FakeStatement>(NUM_STATEMENTS);
        EXPECT_THROW(strat.readEach(yield, statements), DatabaseTimeout);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, WriteSyncFirstTrySuccessful)
{
    auto strat = makeStrategy();

    ON_CALL(handle_, execute(A<FakeStatement const&>())).WillByDefault([](auto const&) { return FakeResultOrError{}; });
    EXPECT_CALL(handle_,
                execute(A<FakeStatement const&>())).Times(1);  // first one will succeed
    EXPECT_CALL(*counters_, registerWriteSync());

    EXPECT_TRUE(strat.writeSync({}));
}

TEST_F(BackendCassandraExecutionStrategyTest, WriteSyncRetrySuccessful)
{
    auto strat = makeStrategy();
    auto callCount = 0;

    ON_CALL(handle_, execute(A<FakeStatement const&>())).WillByDefault([&callCount](auto const&) {
        if (callCount++ == 1)
            return FakeResultOrError{};
        return FakeResultOrError{CassandraError{"invalid data", CASS_ERROR_LIB_INVALID_DATA}};
    });
    EXPECT_CALL(handle_,
                execute(A<FakeStatement const&>())).Times(2);  // first one will fail, second will succeed
    EXPECT_CALL(*counters_, registerWriteSyncRetry());
    EXPECT_CALL(*counters_, registerWriteSync());

    EXPECT_TRUE(strat.writeSync({}));
}

TEST_F(BackendCassandraExecutionStrategyTest, WriteMultipleAndCallSyncSucceeds)
{
    auto strat = makeStrategy();
    auto const totalRequests = 1024u;
    auto callCount = std::atomic_uint{0u};

    auto work = std::optional<boost::asio::io_context::work>{ctx};
    auto thread = std::thread{[this]() { ctx.run(); }};

    ON_CALL(
        handle_, asyncExecute(A<std::vector<FakeStatement> const&>(), A<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([this, &callCount](auto const&, auto&& cb) {
            // run on thread to emulate concurrency model of real asyncExecute
            boost::asio::post(ctx, [&callCount, cb = std::forward<decltype(cb)>(cb)] {
                ++callCount;
                cb({});  // pretend we got data
            });
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(
        handle_,
        asyncExecute(
            A<std::vector<FakeStatement> const&>(),
            A<std::function<void(FakeResultOrError)>&&>()))
        .Times(totalRequests);  // one per write call
    EXPECT_CALL(*counters_, registerWriteStarted()).Times(totalRequests);
    EXPECT_CALL(*counters_, registerWriteFinished()).Times(totalRequests);

    auto makeStatements = [] { return std::vector<FakeStatement>(16); };
    for (auto i = 0u; i < totalRequests; ++i)
        strat.write(makeStatements());

    strat.sync();                         // make sure all above writes are finished
    EXPECT_EQ(callCount, totalRequests);  // all requests should finish

    work.reset();
    thread.join();
}

TEST_F(BackendCassandraExecutionStrategyTest, StatsCallsCountersReport)
{
    auto strat = makeStrategy();
    EXPECT_CALL(*counters_, report());
    strat.stats();
}
