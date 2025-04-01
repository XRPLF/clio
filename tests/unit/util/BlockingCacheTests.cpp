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
#include <condition_variable>
#include <expected>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using testing::MockFunction;
using testing::Return;
using testing::StrictMock;

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
        EXPECT_EQ(result.error(), "Failed to update cache");
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

TEST_F(BlockingCacheTests, TimeoutReturnsFailure)
{
    EXPECT_CALL(mockUpdater, Call).WillOnce([](boost::asio::yield_context yield) -> std::expected<int, std::string> {
        // Never completes within timeout
        boost::asio::steady_timer timer{yield.get_executor(), std::chrono::seconds(10)};
        boost::system::error_code ec;
        timer.async_wait(yield[ec]);
        return 0;  // Never reached due to timeout
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = cache.asyncGet(
            yield,
            mockVerifier.AsStdFunction(),
            mockUpdater.AsStdFunction(),
            std::chrono::milliseconds(10)  // Short timeout
        );

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), "Waiting timeout");
    });
}

/*
TEST_F(BlockingCacheTests, ConcurrentRequestsBlockUntilUpdate)
{
    MockUpdater slowUpdater;
    EXPECT_CALL(slowUpdater, Call)
        .WillOnce([this](boost::asio::yield_context yield) -> std::expected<int, std::string> {
            // Simulate slow response
            boost::asio::steady_timer timer{ctx_, std::chrono::milliseconds(50)};
            boost::system::error_code ec;
            timer.async_wait(yield[ec]);
            return value;
        });

    EXPECT_CALL(mockVerifier, Call(value)).WillRepeatedly(Return(true));

    // Second updater should not be called because the first update will complete
    // and subsequent requests should use the cached value
    EXPECT_CALL(mockUpdater, Call).Times(0);

    runSpawnWithTimeout(1s, [&](boost::asio::yield_context yield) {
        // Start the first update that will take time
        boost::asio::spawn(ctx_, [&](boost::asio::yield_context innerYield) {
            auto const result = cache.asyncGet(innerYield, mockVerifier.AsStdFunction(), slowUpdater.AsStdFunction(),
std::nullopt); ASSERT_TRUE(result.has_value()); EXPECT_EQ(result.value(), value);
        });

        // Give the first coroutine time to start
        boost::asio::steady_timer timer{ctx_, std::chrono::milliseconds(10)};
        boost::system::error_code ec;
        timer.async_wait(yield[ec]);

        // Second request shouldn't need to call the updater
        auto const result = cache.asyncGet(yield, mockVerifier.AsStdFunction(), mockUpdater.AsStdFunction(),
std::nullopt);

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), value);
    });
}

TEST_F(BlockingCacheTests, MultipleConsecutiveUpdates)
{
    int const newValue = 456;

    // First call - populate cache
    EXPECT_CALL(mockVerifier, Call(value)).WillOnce(Return(true));
    EXPECT_CALL(mockUpdater, Call).WillOnce(Return(value));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = cache.asyncGet(yield, mockVerifier.AsStdFunction(), mockUpdater.AsStdFunction(),
std::nullopt); ASSERT_TRUE(result.has_value()); EXPECT_EQ(result.value(), value);
    });

    // Second call with a different updater function
    MockUpdater secondUpdater;
    EXPECT_CALL(secondUpdater, Call).WillOnce(Return(newValue));

    StrictMock<MockFunction<bool(int const&)>> secondVerifier;
    EXPECT_CALL(secondVerifier, Call(newValue)).WillOnce(Return(true));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = cache.asyncGet(yield, secondVerifier.AsStdFunction(), secondUpdater.AsStdFunction(),
std::nullopt); ASSERT_TRUE(result.has_value()); EXPECT_EQ(result.value(), newValue);
    });
}

TEST_F(BlockingCacheTests, ValueDoesntChangeOnTimeoutDuringUpdate)
{
    // Initialize cache with a value
    util::BlockingCache<int> cacheWithValue{value};

    MockUpdater slowUpdater;
    EXPECT_CALL(slowUpdater, Call)
        .WillOnce([](boost::asio::yield_context yield) -> std::expected<int, std::string> {
            // Simulate slow updater
            boost::asio::steady_timer timer{yield.get_executor(), std::chrono::seconds(1)};
            boost::system::error_code ec;
            timer.async_wait(yield[ec]);
            return 456;  // Different value that should never be set due to timeout
        });

    // Verifier accepts both old and new values
    auto verifier = [](int const&) { return true; };

    // This updater should never be called because the cache already has a valid value
    MockUpdater shouldNotBeCalled;
    EXPECT_CALL(shouldNotBeCalled, Call).Times(0);

    // Set a very short timeout
    runSpawn([&](boost::asio::yield_context yield) {
        auto const result = cacheWithValue.asyncGet(yield, verifier, slowUpdater.AsStdFunction(),
std::chrono::milliseconds(10));

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), "Waiting timeout");

        // Cache should still have the original value
        auto const resultAfterTimeout = cacheWithValue.asyncGet(
            yield, verifier, shouldNotBeCalled.AsStdFunction(), std::nullopt
        );

        ASSERT_TRUE(resultAfterTimeout.has_value());
        EXPECT_EQ(resultAfterTimeout.value(), value);
    });
}
*/
