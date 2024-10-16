//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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
#include "util/CoroutineGroup.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

using namespace util;

struct CoroutineGroupTests : SyncAsioContextTest {
    testing::StrictMock<testing::MockFunction<void()>> callback1_;
    testing::StrictMock<testing::MockFunction<void()>> callback2_;
    testing::StrictMock<testing::MockFunction<void()>> callback3_;
};

TEST_F(CoroutineGroupTests, spawnWait)
{
    testing::Sequence sequence;
    EXPECT_CALL(callback1_, Call).InSequence(sequence);
    EXPECT_CALL(callback2_, Call).InSequence(sequence);
    EXPECT_CALL(callback3_, Call).InSequence(sequence);

    runSpawn([this](boost::asio::yield_context yield) {
        CoroutineGroup group{yield};

        group.spawn(yield, [&](boost::asio::yield_context yield) {
            boost::asio::steady_timer timer{yield.get_executor(), std::chrono::milliseconds{1}};
            timer.async_wait(yield);
            callback1_.Call();
        });
        EXPECT_EQ(group.size(), 1);

        group.spawn(yield, [&](boost::asio::yield_context yield) {
            boost::asio::steady_timer timer{yield.get_executor(), std::chrono::milliseconds{2}};
            timer.async_wait(yield);
            callback2_.Call();
        });
        EXPECT_EQ(group.size(), 2);

        group.asyncWait(yield);
        EXPECT_EQ(group.size(), 0);

        callback3_.Call();
    });
}

TEST_F(CoroutineGroupTests, spawnWaitSpawnWait)
{
    testing::Sequence sequence;
    EXPECT_CALL(callback1_, Call).InSequence(sequence);
    EXPECT_CALL(callback2_, Call).InSequence(sequence);
    EXPECT_CALL(callback3_, Call).InSequence(sequence);

    runSpawn([this](boost::asio::yield_context yield) {
        CoroutineGroup group{yield};

        group.spawn(yield, [&](boost::asio::yield_context yield) {
            boost::asio::steady_timer timer{yield.get_executor(), std::chrono::milliseconds{1}};
            timer.async_wait(yield);
            callback1_.Call();
        });
        EXPECT_EQ(group.size(), 1);

        group.asyncWait(yield);
        EXPECT_EQ(group.size(), 0);

        group.spawn(yield, [&](boost::asio::yield_context yield) {
            boost::asio::steady_timer timer{yield.get_executor(), std::chrono::milliseconds{1}};
            timer.async_wait(yield);
            callback2_.Call();
        });
        EXPECT_EQ(group.size(), 1);

        group.asyncWait(yield);
        EXPECT_EQ(group.size(), 0);

        callback3_.Call();
    });
}
TEST_F(CoroutineGroupTests, childCoroutinesFinishBeforeWait)
{
    testing::Sequence sequence;
    EXPECT_CALL(callback2_, Call).InSequence(sequence);
    EXPECT_CALL(callback1_, Call).InSequence(sequence);
    EXPECT_CALL(callback3_, Call).InSequence(sequence);

    runSpawn([this](boost::asio::yield_context yield) {
        CoroutineGroup group{yield};
        group.spawn(yield, [&](boost::asio::yield_context yield) {
            boost::asio::steady_timer timer{yield.get_executor(), std::chrono::milliseconds{2}};
            timer.async_wait(yield);
            callback1_.Call();
        });
        group.spawn(yield, [&](boost::asio::yield_context yield) {
            boost::asio::steady_timer timer{yield.get_executor(), std::chrono::milliseconds{1}};
            timer.async_wait(yield);
            callback2_.Call();
        });

        boost::asio::steady_timer timer{yield.get_executor(), std::chrono::milliseconds{3}};
        timer.async_wait(yield);

        group.asyncWait(yield);
        callback3_.Call();
    });
}

TEST_F(CoroutineGroupTests, emptyGroup)
{
    testing::Sequence sequence;
    EXPECT_CALL(callback1_, Call).InSequence(sequence);

    runSpawn([this](boost::asio::yield_context yield) {
        CoroutineGroup group{yield};
        group.asyncWait(yield);
        callback1_.Call();
    });
}
