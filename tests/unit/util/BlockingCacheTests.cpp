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
#include "util/NameGenerator.hpp"
#include "util/Spawn.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using testing::MockFunction;
using testing::Return;
using testing::StrictMock;

#include <boost/asio/spawn.hpp>

#include <expected>
#include <string>

struct BlockingCacheTest : SyncAsioContextTest {
    using ErrorType = std::string;
    using ValueType = int;
    using Cache = util::BlockingCache<ValueType, ErrorType>;
    using MockUpdater = StrictMock<MockFunction<std::expected<ValueType, ErrorType>(boost::asio::yield_context)>>;
    using MockVerifier = StrictMock<MockFunction<bool(ValueType const&)>>;

    std::unique_ptr<Cache> cache = std::make_unique<Cache>();
    MockUpdater mockUpdater;
    MockVerifier mockVerifier;
    int const value = 42;
    std::string error = "some error";
};

TEST_F(BlockingCacheTest, asyncGet_NoValueCacheUpdateSuccess)
{
    EXPECT_CALL(mockUpdater, Call).WillOnce(Return(value));
    EXPECT_CALL(mockVerifier, Call(value)).WillOnce(Return(true));

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = cache->asyncGet(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), 42);
    });
}

TEST_F(BlockingCacheTest, asyncGet_NoValueCacheUpdateFailure)
{
    EXPECT_CALL(mockUpdater, Call).WillOnce(Return(std::unexpected{error}));

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = cache->asyncGet(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), error);
    });
}

TEST_F(BlockingCacheTest, asyncGet_NoValueCacheUpdateSuccessButVerifierRejects)
{
    runSpawn([&](boost::asio::yield_context yield) {
        std::expected<ValueType, ErrorType> result;
        {
            EXPECT_CALL(mockUpdater, Call).WillOnce(Return(value));
            EXPECT_CALL(mockVerifier, Call(value)).WillOnce(Return(false));

            result = cache->asyncGet(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(result.value(), value);
        }

        int const newValue = 24;
        {
            EXPECT_CALL(mockUpdater, Call).WillOnce(Return(newValue));
            EXPECT_CALL(mockVerifier, Call(newValue)).WillOnce(Return(true));

            result = cache->asyncGet(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(result.value(), newValue);
        }

        result = cache->asyncGet(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), newValue);
    });
}

TEST_F(BlockingCacheTest, asyncGet_HasValueCacheReturnsValue)
{
    cache = std::make_unique<Cache>(value);

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = cache->asyncGet(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), value);
    });
}

struct BlockingCacheWaitTestBundle {
    bool updateSuccessful;
    bool verifierAccepts;
    std::string testName;
};

struct BlockingCacheWaitTest : BlockingCacheTest, testing::WithParamInterface<BlockingCacheWaitTestBundle> {};

TEST_P(BlockingCacheWaitTest, WaitForUpdate)
{
    bool waitingCoroutineFinished = false;

    auto waitingCoroutine = [&](boost::asio::yield_context yield) {
        auto result = cache->asyncGet(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        if (GetParam().updateSuccessful) {
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(result.value(), value);
        } else {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), error);
        }
        waitingCoroutineFinished = true;
    };

    EXPECT_CALL(mockUpdater, Call)
        .WillOnce([this, &waitingCoroutine](boost::asio::yield_context yield) -> std::expected<ValueType, ErrorType> {
            util::spawn(yield, waitingCoroutine);
            if (GetParam().updateSuccessful) {
                return value;
            }
            return std::unexpected{error};
        });

    if (GetParam().updateSuccessful)
        EXPECT_CALL(mockVerifier, Call(value)).WillOnce(Return(GetParam().verifierAccepts));

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = cache->asyncGet(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());

        if (GetParam().updateSuccessful) {
            ASSERT_TRUE(result.has_value());
            EXPECT_EQ(result.value(), value);
        } else {
            ASSERT_FALSE(result.has_value());
            EXPECT_EQ(result.error(), error);
        }
        ASSERT_FALSE(waitingCoroutineFinished);
    });
}

INSTANTIATE_TEST_SUITE_P(
    BlockingCacheTest,
    BlockingCacheWaitTest,
    testing::Values(
        BlockingCacheWaitTestBundle{
            .updateSuccessful = true,
            .verifierAccepts = true,
            .testName = "UpdateSucceedsVerifierAccepts"
        },
        BlockingCacheWaitTestBundle{
            .updateSuccessful = true,
            .verifierAccepts = false,
            .testName = "UpdateSucceedsVerifierRejects"
        },
        BlockingCacheWaitTestBundle{.updateSuccessful = false, .verifierAccepts = false, .testName = "UpdateFails"}
    ),
    tests::util::kNAME_GENERATOR
);

TEST_F(BlockingCacheTest, InvalidateWhenStateIsNoValue)
{
    ASSERT_EQ(cache->state(), Cache::State::NoValue);
    cache->invalidate();
    ASSERT_EQ(cache->state(), Cache::State::NoValue);
}

TEST_F(BlockingCacheTest, InvalidateWhenStateIsUpdating)
{
    EXPECT_CALL(mockUpdater, Call).WillOnce([this](auto&&) {
        EXPECT_EQ(cache->state(), Cache::State::Updating);
        cache->invalidate();
        EXPECT_EQ(cache->state(), Cache::State::Updating);
        return value;
    });
    EXPECT_CALL(mockVerifier, Call(value)).WillOnce(Return(true));

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = cache->asyncGet(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value(), value);
        ASSERT_EQ(cache->state(), Cache::State::HasValue);
    });
}

TEST_F(BlockingCacheTest, InvalidateWhenStateIsHasValue)
{
    cache = std::make_unique<Cache>(value);
    ASSERT_EQ(cache->state(), Cache::State::HasValue);
    cache->invalidate();
    EXPECT_EQ(cache->state(), Cache::State::NoValue);
}

TEST_F(BlockingCacheTest, UpdateFromTwoCoroutinesHappensOnlyOnce)
{
    auto waitingCoroutine = [&](boost::asio::yield_context yield) {
        auto result = cache->update(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(result.value(), value);
    };

    EXPECT_CALL(mockUpdater, Call)
        .WillOnce([this, &waitingCoroutine](boost::asio::yield_context yield) -> std::expected<ValueType, ErrorType> {
            util::spawn(yield, waitingCoroutine);
            return value;
        });
    EXPECT_CALL(mockVerifier, Call(value)).WillOnce(Return(true));

    auto updatingCoroutine = [&](boost::asio::yield_context yield) {
        auto const result = cache->update(yield, mockUpdater.AsStdFunction(), mockVerifier.AsStdFunction());
        EXPECT_TRUE(result.has_value());
        ASSERT_EQ(result.value(), value);
    };

    runSpawn([&](boost::asio::yield_context yield) { util::spawn(yield, updatingCoroutine); });
}
