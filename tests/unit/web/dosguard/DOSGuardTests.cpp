#include "util/config/Array.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "web/dosguard/DOSGuard.hpp"
#include "web/dosguard/WeightsInterface.hpp"
#include "web/dosguard/WhitelistHandlerInterface.hpp"

#include <boost/json/object.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>

using namespace testing;
using namespace util;
using namespace std;
using namespace util::config;
using namespace web::dosguard;

struct DOSGuardTest : public virtual ::testing::Test {
    static constexpr auto kJsonData = R"JSON({
        "dos_guard": {
            "max_fetches": 100,
            "max_connections": 2,
            "max_requests": 3,
            "whitelist": [
                "127.0.0.1"
            ]
        }
    })JSON";

    static constexpr auto kIP = "127.0.0.2";

    struct MockWhitelistHandler : WhitelistHandlerInterface {
        MOCK_METHOD(bool, isWhiteListed, (std::string_view ip), (const));
    };
    struct MockWeights : WeightsInterface {
        MOCK_METHOD(size_t, requestWeight, (boost::json::object const& cmd), (const, override));
    };

    ClioConfigDefinition cfg{
        {{"dos_guard.max_fetches", ConfigValue{ConfigType::Integer}.defaultValue(100)},
         {"dos_guard.max_connections", ConfigValue{ConfigType::Integer}.defaultValue(2)},
         {"dos_guard.max_requests", ConfigValue{ConfigType::Integer}.defaultValue(3)},
         {"dos_guard.whitelist", Array{ConfigValue{ConfigType::String}}}}
    };
    NiceMock<MockWhitelistHandler> whitelistHandler;
    StrictMock<MockWeights> weightsMock;
    DOSGuard guard{cfg, whitelistHandler, weightsMock};
    boost::json::object const request;
};

TEST_F(DOSGuardTest, Whitelisting)
{
    EXPECT_CALL(whitelistHandler, isWhiteListed("127.0.0.1")).Times(1).WillOnce(Return(false));
    EXPECT_FALSE(guard.isWhiteListed("127.0.0.1"));
    EXPECT_CALL(whitelistHandler, isWhiteListed("127.0.0.1")).Times(1).WillOnce(Return(true));
    EXPECT_TRUE(guard.isWhiteListed("127.0.0.1"));
}

TEST_F(DOSGuardTest, ConnectionCount)
{
    EXPECT_TRUE(guard.isOk(kIP));
    guard.increment(kIP);  // one connection
    EXPECT_TRUE(guard.isOk(kIP));
    guard.increment(kIP);  // two connections
    EXPECT_TRUE(guard.isOk(kIP));
    guard.increment(kIP);  // > two connections, can't connect more
    EXPECT_FALSE(guard.isOk(kIP));

    guard.decrement(kIP);
    EXPECT_TRUE(guard.isOk(kIP));  // can connect again
}

TEST_F(DOSGuardTest, FetchCount)
{
    EXPECT_TRUE(guard.add(kIP, 50));  // half of allowance
    EXPECT_TRUE(guard.add(kIP, 50));  // now fully charged
    EXPECT_FALSE(guard.add(kIP, 1));  // can't add even 1 anymore
    EXPECT_FALSE(guard.isOk(kIP));

    guard.clear();                 // force clear the above fetch count
    EXPECT_TRUE(guard.isOk(kIP));  // can fetch again
}

TEST_F(DOSGuardTest, ClearFetchCountOnTimer)
{
    EXPECT_TRUE(guard.add(kIP, 50));  // half of allowance
    EXPECT_TRUE(guard.add(kIP, 50));  // now fully charged
    EXPECT_FALSE(guard.add(kIP, 1));  // can't add even 1 anymore
    EXPECT_FALSE(guard.isOk(kIP));

    guard.clear();                 // pretend sweep called from timer
    EXPECT_TRUE(guard.isOk(kIP));  // can fetch again
}

TEST_F(DOSGuardTest, RequestLimit)
{
    EXPECT_CALL(weightsMock, requestWeight(request)).WillOnce(Return(1));
    EXPECT_TRUE(guard.request(kIP, request));

    EXPECT_CALL(weightsMock, requestWeight(request)).WillOnce(Return(1));
    EXPECT_TRUE(guard.request(kIP, request));

    EXPECT_CALL(weightsMock, requestWeight(request)).WillOnce(Return(1));
    EXPECT_TRUE(guard.request(kIP, request));

    EXPECT_TRUE(guard.isOk(kIP));

    EXPECT_CALL(weightsMock, requestWeight(request)).WillOnce(Return(1));
    EXPECT_FALSE(guard.request(kIP, request));

    EXPECT_FALSE(guard.isOk(kIP));
    guard.clear();
    EXPECT_TRUE(guard.isOk(kIP));  // can request again
}

TEST_F(DOSGuardTest, RequestLimitOnTimer)
{
    EXPECT_CALL(weightsMock, requestWeight(request)).WillOnce(Return(1));
    EXPECT_TRUE(guard.request(kIP, request));

    EXPECT_CALL(weightsMock, requestWeight(request)).WillOnce(Return(1));
    EXPECT_TRUE(guard.request(kIP, request));

    EXPECT_CALL(weightsMock, requestWeight(request)).WillOnce(Return(1));
    EXPECT_TRUE(guard.request(kIP, request));

    EXPECT_TRUE(guard.isOk(kIP));

    EXPECT_CALL(weightsMock, requestWeight(request)).WillOnce(Return(1));
    EXPECT_FALSE(guard.request(kIP, request));

    EXPECT_FALSE(guard.isOk(kIP));
    guard.clear();
    EXPECT_TRUE(guard.isOk(kIP));  // can request again
}
