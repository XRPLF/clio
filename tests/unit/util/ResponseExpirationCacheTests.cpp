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

#include "rpc/Errors.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockAssert.hpp"
#include "util/ResponseExpirationCache.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <unordered_set>

using namespace util;
using testing::MockFunction;
using testing::Return;
using testing::StrictMock;

struct ResponseExpirationCacheTest : SyncAsioContextTest {
    using MockUpdater = StrictMock<MockFunction<
        std::expected<ResponseExpirationCache::EntryData, ResponseExpirationCache::Error>(boost::asio::yield_context)>>;
    using MockVerifier = StrictMock<MockFunction<bool(ResponseExpirationCache::EntryData const&)>>;

    std::string const cmd = "server_info";
    boost::json::object const obj = {{"some key", "some value"}};
    MockUpdater mockUpdater;
    MockVerifier mockVerifier;
};

TEST_F(ResponseExpirationCacheTest, ShouldCacheDeterminesIfCommandIsCacheable)
{
    std::unordered_set<std::string> const cmds = {cmd, "account_info"};
    ResponseExpirationCache cache{std::chrono::seconds(10), cmds};

    for (auto const& c : cmds) {
        EXPECT_TRUE(cache.shouldCache(c));
    }

    EXPECT_FALSE(cache.shouldCache("account_tx"));
    EXPECT_FALSE(cache.shouldCache("ledger"));
    EXPECT_FALSE(cache.shouldCache("submit"));
    EXPECT_FALSE(cache.shouldCache(""));
}

TEST_F(ResponseExpirationCacheTest, ShouldCacheEmptySetMeansNothingCacheable)
{
    std::unordered_set<std::string> const emptyCmds;
    ResponseExpirationCache cache{std::chrono::seconds(10), emptyCmds};

    EXPECT_FALSE(cache.shouldCache("server_info"));
    EXPECT_FALSE(cache.shouldCache("account_info"));
    EXPECT_FALSE(cache.shouldCache("any_command"));
    EXPECT_FALSE(cache.shouldCache(""));
}

TEST_F(ResponseExpirationCacheTest, ShouldCacheCaseMatchingIsRequired)
{
    std::unordered_set<std::string> const specificCmds = {cmd};
    ResponseExpirationCache cache{std::chrono::seconds(10), specificCmds};

    EXPECT_TRUE(cache.shouldCache(cmd));
    EXPECT_FALSE(cache.shouldCache("SERVER_INFO"));
    EXPECT_FALSE(cache.shouldCache("Server_Info"));
}

TEST_F(ResponseExpirationCacheTest, GetOrUpdateNoValueInCacheCallsUpdaterAndVerifier)
{
    ResponseExpirationCache cache{std::chrono::seconds(10), {cmd}};

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_CALL(mockUpdater, Call)
            .WillOnce(Return(ResponseExpirationCache::EntryData{
                .lastUpdated = std::chrono::steady_clock::now(),
                .response = obj,
            }));
        EXPECT_CALL(mockVerifier, Call).WillOnce(Return(true));

        auto result =
            cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), obj);
    });
}

TEST_F(ResponseExpirationCacheTest, GetOrUpdateExpiredValueInCacheCallsUpdaterAndVerifier)
{
    ResponseExpirationCache cache{std::chrono::milliseconds(1), {cmd}};

    runSpawn([&](boost::asio::yield_context yield) {
        boost::json::object const expiredObject = {{"some key", "expired value"}};
        EXPECT_CALL(mockUpdater, Call)
            .WillOnce(Return(ResponseExpirationCache::EntryData{
                .lastUpdated = std::chrono::steady_clock::now(),
                .response = expiredObject,
            }));
        EXPECT_CALL(mockVerifier, Call).WillOnce(Return(true));

        auto result =
            cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), expiredObject);

        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        EXPECT_CALL(mockUpdater, Call)
            .WillOnce(Return(
                ResponseExpirationCache::EntryData{.lastUpdated = std::chrono::steady_clock::now(), .response = obj}
            ));
        EXPECT_CALL(mockVerifier, Call).WillOnce(Return(true));

        result = cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), obj);
    });
}

TEST_F(ResponseExpirationCacheTest, GetOrUpdateCachedValueNotExpiredDoesNotCallUpdaterOrVerifier)
{
    ResponseExpirationCache cache{std::chrono::seconds(10), {cmd}};

    runSpawn([&](boost::asio::yield_context yield) {
        // First call to populate cache
        EXPECT_CALL(mockUpdater, Call)
            .WillOnce(Return(ResponseExpirationCache::EntryData{
                .lastUpdated = std::chrono::steady_clock::now(),
                .response = obj,
            }));
        EXPECT_CALL(mockVerifier, Call).WillOnce(Return(true));

        auto result =
            cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), obj);

        // Second call should use cached value and not call updater/verifier
        result = cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), obj);
    });
}

TEST_F(ResponseExpirationCacheTest, GetOrUpdateHandlesErrorFromUpdater)
{
    ResponseExpirationCache cache{std::chrono::seconds(10), {cmd}};

    ResponseExpirationCache::Error const error{
        .status = rpc::Status{rpc::ClioError::EtlConnectionError}, .warnings = {}
    };

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_CALL(mockUpdater, Call).WillOnce(Return(std::unexpected(error)));

        auto result =
            cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), error);
    });
}

TEST_F(ResponseExpirationCacheTest, GetOrUpdateVerifierRejection)
{
    ResponseExpirationCache cache{std::chrono::seconds(10), {cmd}};

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_CALL(mockUpdater, Call)
            .WillOnce(Return(ResponseExpirationCache::EntryData{
                .lastUpdated = std::chrono::steady_clock::now(),
                .response = obj,
            }));
        EXPECT_CALL(mockVerifier, Call).WillOnce(Return(false));

        auto result =
            cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), obj);

        boost::json::object const anotherObj = {{"some key", "another value"}};
        EXPECT_CALL(mockUpdater, Call)
            .WillOnce(Return(ResponseExpirationCache::EntryData{
                .lastUpdated = std::chrono::steady_clock::now(),
                .response = anotherObj,
            }));
        EXPECT_CALL(mockVerifier, Call).WillOnce(Return(true));

        result = cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), anotherObj);
    });
}

TEST_F(ResponseExpirationCacheTest, GetOrUpdateMultipleConcurrentUpdates)
{
    ResponseExpirationCache cache{std::chrono::seconds(10), {cmd}};
    bool waitingCoroutineFinished = false;

    auto waitingCoroutine = [&](boost::asio::yield_context yield) {
        auto result =
            cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), obj);
        waitingCoroutineFinished = true;
    };

    EXPECT_CALL(mockUpdater, Call)
        .WillOnce(
            [this, &waitingCoroutine](boost::asio::yield_context yield
            ) -> std::expected<ResponseExpirationCache::EntryData, ResponseExpirationCache::Error> {
                boost::asio::spawn(yield, waitingCoroutine);
                return ResponseExpirationCache::EntryData{
                    .lastUpdated = std::chrono::steady_clock::now(),
                    .response = obj,
                };
            }
        );
    EXPECT_CALL(mockVerifier, Call).WillOnce(Return(true));

    runSpawnWithTimeout(std::chrono::seconds{1}, [&](boost::asio::yield_context yield) {
        auto result =
            cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), obj);
        ASSERT_FALSE(waitingCoroutineFinished);
    });
}

TEST_F(ResponseExpirationCacheTest, InvalidateForcesRefresh)
{
    ResponseExpirationCache cache{std::chrono::seconds(10), {cmd}};

    runSpawn([&](boost::asio::yield_context yield) {
        boost::json::object const oldObject = {{"some key", "old value"}};
        EXPECT_CALL(mockUpdater, Call)
            .WillOnce(Return(ResponseExpirationCache::EntryData{
                .lastUpdated = std::chrono::steady_clock::now(),
                .response = oldObject,
            }));
        EXPECT_CALL(mockVerifier, Call).WillOnce(Return(true));

        auto result =
            cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), oldObject);

        cache.invalidate();

        EXPECT_CALL(mockUpdater, Call)
            .WillOnce(Return(ResponseExpirationCache::EntryData{
                .lastUpdated = std::chrono::steady_clock::now(),
                .response = obj,
            }));
        EXPECT_CALL(mockVerifier, Call).WillOnce(Return(true));

        result = cache.getOrUpdate(yield, "server_info", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), obj);
    });
}

struct ResponseExpirationCacheAssertTest : common::util::WithMockAssert, ResponseExpirationCacheTest {};

TEST_F(ResponseExpirationCacheAssertTest, NonCacheableCommandThrowsAssertion)
{
    ResponseExpirationCache cache{std::chrono::seconds(10), {cmd}};

    ASSERT_FALSE(cache.shouldCache("non_cacheable_command"));

    runSpawn([&](boost::asio::yield_context yield) {
        EXPECT_CLIO_ASSERT_FAIL({
            [[maybe_unused]]
            auto const v = cache.getOrUpdate(
                yield, "non_cacheable_command", mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction()
            );
        });
    });
}
