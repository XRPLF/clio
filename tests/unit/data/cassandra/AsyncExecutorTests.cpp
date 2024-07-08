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

#include "data/cassandra/Error.hpp"
#include "data/cassandra/FakesAndMocks.hpp"
#include "data/cassandra/impl/AsyncExecutor.hpp"
#include "util/AsioContextTestFixture.hpp"

#include <boost/asio/io_context.hpp>
#include <cassandra.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <functional>
#include <optional>
#include <thread>
#include <utility>

using namespace data::cassandra;
using namespace data::cassandra::impl;
using namespace testing;

class BackendCassandraAsyncExecutorTest : public SyncAsioContextTest {
protected:
    struct CallbackMock {
        MOCK_METHOD(void, onComplete, (FakeResultOrError));
        MOCK_METHOD(void, onRetry, ());
    };
    CallbackMock callbackMock_;
    std::function<void()> onRetry_ = [this]() { callbackMock_.onRetry(); };
};

TEST_F(BackendCassandraAsyncExecutorTest, CompletionCalledOnSuccess)
{
    auto handle = MockHandle{};

    ON_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([this](auto const&, auto&& cb) {
            ctx.post([cb = std::forward<decltype(cb)>(cb)]() { cb({}); });
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(AtLeast(1));

    auto work = std::optional<boost::asio::io_context::work>{ctx};
    EXPECT_CALL(callbackMock_, onComplete);

    AsyncExecutor<FakeStatement, MockHandle>::run(
        ctx,
        handle,
        FakeStatement{},
        [&work, this](auto resultOrError) {
            callbackMock_.onComplete(std::move(resultOrError));
            work.reset();
        },
        std::move(onRetry_)
    );

    ctx.run();
}

TEST_F(BackendCassandraAsyncExecutorTest, ExecutedMultipleTimesByRetryPolicyOnMainThread)
{
    auto callCount = std::atomic_int{0};
    auto handle = MockHandle{};

    // emulate successfull execution after some attempts
    ON_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([&callCount](auto const&, auto&& cb) {
            ++callCount;
            if (callCount >= 3) {
                cb({});
            } else {
                cb({CassandraError{"timeout", CASS_ERROR_LIB_REQUEST_TIMED_OUT}});
            }

            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(3);

    auto work = std::optional<boost::asio::io_context::work>{ctx};
    EXPECT_CALL(callbackMock_, onComplete);
    EXPECT_CALL(callbackMock_, onRetry).Times(2);

    AsyncExecutor<FakeStatement, MockHandle>::run(
        ctx,
        handle,
        FakeStatement{},
        [this, &work](auto resultOrError) {
            callbackMock_.onComplete(std::move(resultOrError));
            work.reset();
        },
        std::move(onRetry_)
    );

    ctx.run();
    ASSERT_EQ(callCount, 3);
}

TEST_F(BackendCassandraAsyncExecutorTest, ExecutedMultipleTimesByRetryPolicyOnOtherThread)
{
    auto callCount = std::atomic_int{0};
    auto handle = MockHandle{};

    auto threadedCtx = boost::asio::io_context{};
    auto work = std::optional<boost::asio::io_context::work>{threadedCtx};
    auto thread = std::thread{[&threadedCtx] { threadedCtx.run(); }};

    // emulate successfull execution after some attempts
    ON_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([&callCount](auto const&, auto&& cb) {
            ++callCount;
            if (callCount >= 3) {
                cb({});
            } else {
                cb({CassandraError{"timeout", CASS_ERROR_LIB_REQUEST_TIMED_OUT}});
            }

            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(3);

    auto work2 = std::optional<boost::asio::io_context::work>{ctx};
    EXPECT_CALL(callbackMock_, onComplete);
    EXPECT_CALL(callbackMock_, onRetry).Times(2);

    AsyncExecutor<FakeStatement, MockHandle>::run(
        threadedCtx,
        handle,
        FakeStatement{},
        [this, &work, &work2](auto resultOrError) {
            callbackMock_.onComplete(std::move(resultOrError));
            work.reset();
            work2.reset();
        },
        std::move(onRetry_)
    );

    ctx.run();
    EXPECT_EQ(callCount, 3);
    threadedCtx.stop();
    thread.join();
}

TEST_F(BackendCassandraAsyncExecutorTest, CompletionCalledOnFailureAfterRetryCountExceeded)
{
    auto handle = MockHandle{};

    // FakeRetryPolicy returns false for shouldRetry in which case we should
    // still call onComplete giving it whatever error we have raised internally.
    ON_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .WillByDefault([](auto const&, auto&& cb) {
            cb({CassandraError{"not a timeout", CASS_ERROR_LIB_INTERNAL_ERROR}});
            return FakeFutureWithCallback{};
        });
    EXPECT_CALL(handle, asyncExecute(An<FakeStatement const&>(), An<std::function<void(FakeResultOrError)>&&>()))
        .Times(1);

    auto work = std::optional<boost::asio::io_context::work>{ctx};
    EXPECT_CALL(callbackMock_, onComplete);

    AsyncExecutor<FakeStatement, MockHandle, FakeRetryPolicy>::run(
        ctx,
        handle,
        FakeStatement{},
        [this, &work](auto res) {
            EXPECT_FALSE(res);
            EXPECT_EQ(res.error().code(), CASS_ERROR_LIB_INTERNAL_ERROR);
            EXPECT_EQ(res.error().message(), "not a timeout");

            callbackMock_.onComplete(std::move(res));
            work.reset();
        },
        std::move(onRetry_)
    );

    ctx.run();
}
