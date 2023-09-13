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
};

TEST_F(BackendCassandraExecutionStrategyTest, ReadOneInCoroutineSuccessful)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};

    ON_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const& /* statement */, auto&& cb) {
            cb({});  // pretend we got data
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statement = FakeStatement{};
        strat.read(yield, statement);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadOneInCoroutineThrowsOnTimeoutFailure)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};

    ON_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const&, auto&& cb) {
            auto res = FakeResultOrError{CassandraError{"timeout", CASS_ERROR_LIB_REQUEST_TIMED_OUT}};
            cb(res);  // notify that item is ready
            return FakeFutureWithCallback{res};
        });
    EXPECT_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statement = FakeStatement{};
        EXPECT_THROW(strat.read(yield, statement), DatabaseTimeout);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadOneInCoroutineThrowsOnInvalidQueryFailure)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};

    ON_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const&, auto&& cb) {
            auto res = FakeResultOrError{CassandraError{"invalid", CASS_ERROR_SERVER_INVALID_QUERY}};
            cb(res);  // notify that item is ready
            return FakeFutureWithCallback{res};
        });
    EXPECT_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statement = FakeStatement{};
        EXPECT_THROW(strat.read(yield, statement), std::runtime_error);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadBatchInCoroutineSuccessful)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};

    ON_CALL(
        handle, asyncExecute(An<std::vector<FakeStatement> const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const& statements, auto&& cb) {
            EXPECT_EQ(statements.size(), 3);
            cb({});  // pretend we got data
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(
        handle, asyncExecute(An<std::vector<FakeStatement> const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statements = std::vector<FakeStatement>(3);
        strat.read(yield, statements);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadBatchInCoroutineThrowsOnTimeoutFailure)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};

    ON_CALL(
        handle, asyncExecute(An<std::vector<FakeStatement> const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const& statements, auto&& cb) {
            EXPECT_EQ(statements.size(), 3);
            auto res = FakeResultOrError{CassandraError{"timeout", CASS_ERROR_LIB_REQUEST_TIMED_OUT}};
            cb(res);  // notify that item is ready
            return FakeFutureWithCallback{res};
        });
    EXPECT_CALL(
        handle, asyncExecute(An<std::vector<FakeStatement> const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statements = std::vector<FakeStatement>(3);
        EXPECT_THROW(strat.read(yield, statements), DatabaseTimeout);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadBatchInCoroutineThrowsOnInvalidQueryFailure)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};

    ON_CALL(
        handle, asyncExecute(An<std::vector<FakeStatement> const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const& statements, auto&& cb) {
            EXPECT_EQ(statements.size(), 3);
            auto res = FakeResultOrError{CassandraError{"invalid", CASS_ERROR_SERVER_INVALID_QUERY}};
            cb(res);  // notify that item is ready
            return FakeFutureWithCallback{res};
        });
    EXPECT_CALL(
        handle, asyncExecute(An<std::vector<FakeStatement> const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statements = std::vector<FakeStatement>(3);
        EXPECT_THROW(strat.read(yield, statements), std::runtime_error);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadBatchInCoroutineMarksBusyIfRequestsOutstandingExceeded)
{
    auto handle = MockHandle{};
    auto settings = Settings{};
    settings.maxReadRequestsOutstanding = 2;
    auto strat = DefaultExecutionStrategy{settings, handle};

    ON_CALL(
        handle, asyncExecute(An<std::vector<FakeStatement> const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([&strat](auto const& statements, auto&& cb) {
            EXPECT_EQ(statements.size(), 3);
            EXPECT_TRUE(strat.isTooBusy());  // 2 was the limit, we sent 3

            cb({});  // notify that item is ready
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(
        handle, asyncExecute(An<std::vector<FakeStatement> const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);

    runSpawn([&strat](boost::asio::yield_context yield) {
        EXPECT_FALSE(strat.isTooBusy());  // 2 was the limit, 0 atm
        auto statements = std::vector<FakeStatement>(3);
        strat.read(yield, statements);
        EXPECT_FALSE(strat.isTooBusy());  // after read completes it's 0 again
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadEachInCoroutineSuccessful)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};

    ON_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const&, auto&& cb) {
            cb({});  // pretend we got data
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(
        handle,
        asyncExecute(
            An<FakeStatement const&>(),
            An<std::function<void(FakeResultOrError)>&&>()))
        .Times(3);  // once per statement

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statements = std::vector<FakeStatement>(3);
        auto res = strat.readEach(yield, statements);
        EXPECT_EQ(res.size(), statements.size());
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, ReadEachInCoroutineThrowsOnFailure)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};
    auto callCount = std::atomic_int{0};

    ON_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([&callCount](auto const&, auto&& cb) {
            if (callCount == 1)  // error happens on one of the entries
                cb({CassandraError{"invalid data", CASS_ERROR_LIB_INVALID_DATA}});
            else
                cb({});  // pretend we got data
            ++callCount;
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(
        handle,
        asyncExecute(
            An<FakeStatement const&>(),
            An<std::function<void(FakeResultOrError)>&&>()))
        .Times(3);  // once per statement

    runSpawn([&strat](boost::asio::yield_context yield) {
        auto statements = std::vector<FakeStatement>(3);
        EXPECT_THROW(strat.readEach(yield, statements), DatabaseTimeout);
    });
}

TEST_F(BackendCassandraExecutionStrategyTest, WriteSyncFirstTrySuccessful)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};

    ON_CALL(handle, execute(An<FakeStatement const&>())).WillByDefault([](auto const&) { return FakeResultOrError{}; });
    EXPECT_CALL(handle,
                execute(An<FakeStatement const&>())).Times(1);  // first one will succeed

    EXPECT_TRUE(strat.writeSync({}));
}

TEST_F(BackendCassandraExecutionStrategyTest, WriteSyncRetrySuccessful)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};
    auto callCount = 0;

    ON_CALL(handle, execute(An<FakeStatement const&>())).WillByDefault([&callCount](auto const&) {
        if (callCount++ == 1)
            return FakeResultOrError{};
        return FakeResultOrError{CassandraError{"invalid data", CASS_ERROR_LIB_INVALID_DATA}};
    });
    EXPECT_CALL(handle,
                execute(An<FakeStatement const&>())).Times(2);  // first one will fail, second will succeed

    EXPECT_TRUE(strat.writeSync({}));
}

TEST_F(BackendCassandraExecutionStrategyTest, WriteMultipleAndCallSyncSucceeds)
{
    auto handle = MockHandle{};
    auto strat = DefaultExecutionStrategy{Settings{}, handle};
    auto totalRequests = 1024u;
    auto callCount = std::atomic_uint{0u};

    auto work = std::optional<boost::asio::io_context::work>{ctx};
    auto thread = std::thread{[this]() { ctx.run(); }};

    ON_CALL(
        handle, asyncExecute(An<std::vector<FakeStatement> const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([this, &callCount](auto const&, auto&& cb) {
            // run on thread to emulate concurrency model of real asyncExecute
            boost::asio::post(ctx, [&callCount, cb = std::move(cb)] {
                ++callCount;
                cb({});  // pretend we got data
            });
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(
        handle,
        asyncExecute(
            An<std::vector<FakeStatement> const&>(),
            An<std::function<void(FakeResultOrError)>&&>()))
        .Times(totalRequests);  // one per write call

    auto makeStatements = [] { return std::vector<FakeStatement>(16); };
    for (auto i = 0u; i < totalRequests; ++i)
        strat.write(makeStatements());

    strat.sync();                         // make sure all above writes are finished
    EXPECT_EQ(callCount, totalRequests);  // all requests should finish

    work.reset();
    thread.join();
}
