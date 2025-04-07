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
#include "util/BlockingCache.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <expected>
#include <optional>
#include <string>

using namespace std::chrono_literals;
using testing::MockFunction;
using testing::Return;
using testing::StrictMock;

/*
struct BlockingCacheTests : SyncAsioContextTest {
    util::BlockingCache<int> cache;
    int const value = 123;
    StrictMock<MockFunction<std::expected<int, std::string>(boost::asio::yield_context)>> mockUpdater;
    StrictMock<MockFunction<bool(int const&)>> mockVerifier;
};

TEST_F(BlockingCacheTests, GetValueWhenValueIsInCacheAndValid)
{
    util::BlockingCache<int> cacheWithValue{value};
    EXPECT_CALL(mockVerifier, Call(value)).WillOnce(Return(true));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result =
            cacheWithValue.asyncGet(yield, mockVerifier.AsStdFunction(), mockUpdater.AsStdFunction(), std::nullopt);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), value);
    });
}

TEST_F(BlockingCacheTests, GetValueWhenCacheIsValid)
{
    EXPECT_CALL(mockVerifier, Call(value)).WillOnce(Return(true));  // Second call succeeds

    EXPECT_CALL(mockUpdater, Call).WillOnce(Return(value));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result =
            cache.asyncGet(yield, mockVerifier.AsStdFunction(), mockUpdater.AsStdFunction(), std::nullopt);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), value);
    });
}

TEST_F(BlockingCacheTests, FailsWhenVerifierRejectsValue)
{
    EXPECT_CALL(mockVerifier, Call(value)).WillOnce(Return(false));

    EXPECT_CALL(mockUpdater, Call).WillOnce(Return(value));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result =
            cache.asyncGet(yield, mockVerifier.AsStdFunction(), mockUpdater.AsStdFunction(), std::nullopt);

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), "Invalid value after update");
    });
}

TEST_F(BlockingCacheTests, UpdaterFailurePropagates)
{
    EXPECT_CALL(mockUpdater, Call).WillOnce(Return(std::unexpected<std::string>("Update failed")));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result =
            cache.asyncGet(yield, mockVerifier.AsStdFunction(), mockUpdater.AsStdFunction(), std::nullopt);

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), "Update failed");
    });
}

TEST_F(BlockingCacheTests, SecondCoroutineWaitsForUpdate)
{
    testing::Expectation const updateExpectation =
        EXPECT_CALL(mockUpdater, Call).WillOnce([&](boost::asio::yield_context yield) {
            boost::asio::steady_timer timer{ctx_, std::chrono::milliseconds{10}};
            timer.async_wait(yield);
            return std::expected<int, std::string>{value};
        });

    EXPECT_CALL(mockVerifier, Call(value)).Times(2).After(updateExpectation).WillRepeatedly(Return(true));

    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto const result =
            cache.asyncGet(yield, mockVerifier.AsStdFunction(), mockUpdater.AsStdFunction(), std::nullopt);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), value);
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result =
            cache.asyncGet(yield, mockVerifier.AsStdFunction(), mockUpdater.AsStdFunction(), std::nullopt);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), value);
    });
}

TEST_F(BlockingCacheTests, SecondCoroutineTimesOut)
{
    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        EXPECT_CALL(mockUpdater, Call).WillOnce([&](boost::asio::yield_context) {
            boost::asio::steady_timer timer{ctx_, std::chrono::milliseconds{10}};
            timer.async_wait(yield);
            return std::expected<int, std::string>{value};
        });
        EXPECT_CALL(mockVerifier, Call(value)).WillOnce(Return(true));
        auto const result =
            cache.asyncGet(yield, mockVerifier.AsStdFunction(), mockUpdater.AsStdFunction(), std::nullopt);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), value);
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = cache.asyncGet(
            yield, mockVerifier.AsStdFunction(), mockUpdater.AsStdFunction(), std::chrono::milliseconds{5}
        );

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), "Waiting timeout");
    });
}
*/
