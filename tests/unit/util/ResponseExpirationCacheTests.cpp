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

#include "util/ResponseExpirationCache.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace util;

struct ResponseExpirationCacheTests : public ::testing::Test {
protected:
    ResponseExpirationCache cache_{std::chrono::seconds{100}, {"key"}};
    boost::json::object object_{{"key", "value"}};
};

TEST_F(ResponseExpirationCacheTests, PutAndGetNotExpired)
{
    EXPECT_FALSE(cache_.get("key").has_value());

    cache_.put("key", object_);
    auto result = cache_.get("key");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, object_);
    result = cache_.get("key2");
    ASSERT_FALSE(result.has_value());

    cache_.put("key2", object_);
    result = cache_.get("key2");
    ASSERT_FALSE(result.has_value());
}

TEST_F(ResponseExpirationCacheTests, Invalidate)
{
    cache_.put("key", object_);
    cache_.invalidate();
    EXPECT_FALSE(cache_.get("key").has_value());
}

TEST_F(ResponseExpirationCacheTests, GetExpired)
{
    ResponseExpirationCache cache{std::chrono::milliseconds{1}, {"key"}};
    auto const response = boost::json::object{{"key", "value"}};

    cache.put("key", response);
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    auto const result = cache.get("key");
    EXPECT_FALSE(result);
}
