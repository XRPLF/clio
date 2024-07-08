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

#include "etl/impl/ForwardingCache.hpp"

#include <boost/json/object.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace etl::impl;

struct CacheEntryTests : public ::testing::Test {
    CacheEntry entry_;
    boost::json::object const object_ = {{"key", "value"}};
};

TEST_F(CacheEntryTests, PutAndGet)
{
    EXPECT_FALSE(entry_.get());

    entry_.put(object_);
    auto result = entry_.get();

    ASSERT_TRUE(result);
    EXPECT_EQ(*result, object_);
}

TEST_F(CacheEntryTests, LastUpdated)
{
    EXPECT_EQ(entry_.lastUpdated().time_since_epoch().count(), 0);

    entry_.put(object_);
    auto const lastUpdated = entry_.lastUpdated();

    EXPECT_GE(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastUpdated).count(), 0
    );

    entry_.put(boost::json::object{{"key", "new value"}});
    auto const newLastUpdated = entry_.lastUpdated();
    EXPECT_GT(newLastUpdated, lastUpdated);
    EXPECT_GE(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - newLastUpdated)
            .count(),
        0
    );
}

TEST_F(CacheEntryTests, Invalidate)
{
    entry_.put(object_);
    entry_.invalidate();
    EXPECT_FALSE(entry_.get());
}

TEST(ForwardingCacheTests, ShouldCache)
{
    for (auto const& command : ForwardingCache::CACHEABLE_COMMANDS) {
        auto const request = boost::json::object{{"command", command}};
        EXPECT_TRUE(ForwardingCache::shouldCache(request));
    }
    auto const request = boost::json::object{{"command", "tx"}};
    EXPECT_FALSE(ForwardingCache::shouldCache(request));

    auto const requestWithoutCommand = boost::json::object{{"key", "value"}};
    EXPECT_FALSE(ForwardingCache::shouldCache(requestWithoutCommand));
}

TEST(ForwardingCacheTests, Get)
{
    ForwardingCache cache{std::chrono::seconds{100}};
    auto const request = boost::json::object{{"command", "server_info"}};
    auto const response = boost::json::object{{"key", "value"}};

    cache.put(request, response);
    auto const result = cache.get(request);

    ASSERT_TRUE(result);
    EXPECT_EQ(*result, response);
}

TEST(ForwardingCacheTests, GetExpired)
{
    ForwardingCache cache{std::chrono::milliseconds{1}};
    auto const request = boost::json::object{{"command", "server_info"}};
    auto const response = boost::json::object{{"key", "value"}};

    cache.put(request, response);
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    auto const result = cache.get(request);
    EXPECT_FALSE(result);
}

TEST(ForwardingCacheTests, GetAndPutNotCommand)
{
    ForwardingCache cache{std::chrono::seconds{100}};
    auto const request = boost::json::object{{"key", "value"}};
    auto const response = boost::json::object{{"key", "value"}};
    cache.put(request, response);
    auto const result = cache.get(request);
    EXPECT_FALSE(result);
}

TEST(ForwardingCache, Invalidate)
{
    ForwardingCache cache{std::chrono::seconds{100}};
    auto const request = boost::json::object{{"command", "server_info"}};
    auto const response = boost::json::object{{"key", "value"}};

    cache.put(request, response);
    cache.invalidate();

    EXPECT_FALSE(cache.get(request));
}
