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

#include "util/AsioContextTestFixture.hpp"
#include "util/AsyncMutex.hpp"
#include "util/MockAssert.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <utility>

using namespace util;

struct AsyncMutexTest : SyncAsioContextTest {};

TEST_F(AsyncMutexTest, MoveUnlockedMutex)
{
    testing::StrictMock<testing::MockFunction<void()>> coroutineCompleted;
    EXPECT_CALL(coroutineCompleted, Call);

    AsyncMutex mutex{ctx_.get_executor()};
    AsyncMutex newMutex{std::move(mutex)};

    runSpawn([&newMutex, &coroutineCompleted](boost::asio::yield_context yield) {
        auto lock = newMutex.lock(yield);
        coroutineCompleted.Call();
    });
}

struct AsyncMutexAssertTest : common::util::WithMockAssert, AsyncMutexTest {};

TEST_F(AsyncMutexAssertTest, MoveLockedMutex)
{
    EXPECT_CLIO_ASSERT_FAIL({
        AsyncMutex mutex{ctx_.get_executor()};
        boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
            auto lock = mutex.lock(yield);
            [[maybe_unused]] AsyncMutex newMutex{std::move(mutex)};
        });
        ctx_.run();
    });
}

TEST_F(AsyncMutexTest, LockProvidesMutualExclusion)
{
    testing::StrictMock<testing::MockFunction<void()>> coroutine1Completed;
    testing::StrictMock<testing::MockFunction<void()>> coroutine2Completed;

    testing::Sequence sequence;
    EXPECT_CALL(coroutine1Completed, Call).InSequence(sequence);
    EXPECT_CALL(coroutine2Completed, Call).InSequence(sequence);

    AsyncMutex mutex{ctx_.get_executor()};
    runSpawn([&](boost::asio::yield_context yield) {
        boost::asio::spawn(yield, [&mutex, &coroutine1Completed](boost::asio::yield_context innerYield) {
            auto const lock = mutex.lock(innerYield);
            boost::asio::steady_timer timer(innerYield.get_executor(), std::chrono::milliseconds{5});
            timer.async_wait(innerYield);
            coroutine1Completed.Call();
        });
        boost::asio::spawn(yield, [&mutex, &coroutine2Completed](boost::asio::yield_context innerYield) {
            auto const lock = mutex.lock(innerYield);
            coroutine2Completed.Call();
        });
    });
}

TEST_F(AsyncMutexTest, MultipleWaitersAreUnblockedSequentially)
{
    testing::StrictMock<testing::MockFunction<void(int)>> coroutineCompleted;

    testing::Sequence sequence;
    EXPECT_CALL(coroutineCompleted, Call(1)).InSequence(sequence);
    EXPECT_CALL(coroutineCompleted, Call(2)).InSequence(sequence);
    EXPECT_CALL(coroutineCompleted, Call(3)).InSequence(sequence);
    EXPECT_CALL(coroutineCompleted, Call(4)).InSequence(sequence);

    AsyncMutex mutex{ctx_.get_executor()};

    auto makeCoroutine = [&](int id) {
        return [&mutex, &coroutineCompleted, id](boost::asio::yield_context yield) {
            auto const lock = mutex.lock(yield);
            boost::asio::steady_timer timer(yield.get_executor(), std::chrono::milliseconds{1});
            timer.async_wait(yield);
            coroutineCompleted.Call(id);
        };
    };

    runSpawn([&](boost::asio::yield_context yield) {
        boost::asio::spawn(yield, makeCoroutine(1));
        boost::asio::spawn(yield, makeCoroutine(2));
        boost::asio::spawn(yield, makeCoroutine(3));
        boost::asio::spawn(yield, makeCoroutine(4));
    });
}
