#include "util/ResponseExpirationCache.hpp"

#include <boost/json/object.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace util;

struct ResponseExpirationCacheTests : public ::testing::Test {
protected:
    ResponseExpirationCache cache_{std::chrono::seconds{100}, {"key"}};
    boost::json::object bareRequest_{{"command", "key"}};
    boost::json::object object_{{"key", "value"}};
};

TEST_F(ResponseExpirationCacheTests, PutAndGetNotExpired)
{
    EXPECT_FALSE(cache_.get("key", bareRequest_).has_value());

    cache_.put("key", bareRequest_, object_);
    auto result = cache_.get("key", bareRequest_);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, object_);  // NOLINT(bugprone-unchecked-optional-access)
    result = cache_.get("key2", bareRequest_);
    ASSERT_FALSE(result.has_value());

    cache_.put("key2", bareRequest_, object_);
    result = cache_.get("key2", bareRequest_);
    ASSERT_FALSE(result.has_value());
}

TEST_F(ResponseExpirationCacheTests, Invalidate)
{
    cache_.put("key", bareRequest_, object_);
    cache_.invalidate();
    EXPECT_FALSE(cache_.get("key", bareRequest_).has_value());
}

TEST_F(ResponseExpirationCacheTests, GetExpired)
{
    ResponseExpirationCache cache{std::chrono::milliseconds{1}, {"key"}};
    auto const response = boost::json::object{{"key", "value"}};
    auto const req = boost::json::object{{"command", "key"}};

    cache.put("key", req, response);
    std::this_thread::sleep_for(std::chrono::milliseconds{2});

    auto const result = cache.get("key", req);
    EXPECT_FALSE(result);
}

TEST_F(ResponseExpirationCacheTests, BareRequestCommandOnlyIsCached)
{
    boost::json::object const req{{"command", "key"}};
    cache_.put("key", req, object_);
    auto const result = cache_.get("key", req);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, object_);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST_F(ResponseExpirationCacheTests, BareRequestWithIdIsCached)
{
    boost::json::object const req{{"command", "key"}, {"id", 42}};
    cache_.put("key", req, object_);
    auto const result = cache_.get("key", req);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, object_);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST_F(ResponseExpirationCacheTests, BareRequestMethodAndIdIsCached)
{
    boost::json::object const req{{"method", "key"}, {"id", "req-1"}};
    cache_.put("key", req, object_);
    auto const result = cache_.get("key", req);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, object_);  // NOLINT(bugprone-unchecked-optional-access)
}

TEST_F(ResponseExpirationCacheTests, NonBareGetReturnNulloptAfterBarePut)
{
    boost::json::object const bareReq{{"command", "key"}};
    boost::json::object const nonBareReq{{"command", "key"}, {"limit", 50}};

    cache_.put("key", bareReq, object_);
    ASSERT_TRUE(cache_.get("key", bareReq).has_value());
    EXPECT_FALSE(cache_.get("key", nonBareReq).has_value());
}

TEST_F(ResponseExpirationCacheTests, NonBarePutDoesNotStore)
{
    boost::json::object const nonBareReq{{"command", "key"}, {"limit", 50}};
    boost::json::object const bareReq{{"command", "key"}};

    cache_.put("key", nonBareReq, object_);
    EXPECT_FALSE(cache_.get("key", bareReq).has_value());
}

TEST_F(ResponseExpirationCacheTests, ApiVersionMakesRequestNonBare)
{
    boost::json::object const req{{"command", "key"}, {"api_version", 1}};
    cache_.put("key", req, object_);
    EXPECT_FALSE(cache_.get("key", req).has_value());
}

TEST_F(ResponseExpirationCacheTests, BareRequestIdOnlyIsCached)
{
    boost::json::object const req{{"id", 7}};
    cache_.put("key", req, object_);
    auto const result = cache_.get("key", req);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, object_);  // NOLINT(bugprone-unchecked-optional-access)
}
