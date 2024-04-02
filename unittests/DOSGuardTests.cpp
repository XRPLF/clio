//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "util/Fixtures.hpp"
#include "util/config/Config.hpp"
#include "web/DOSGuard.hpp"

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace testing;
using namespace util;
using namespace std;
using namespace web;
namespace json = boost::json;

namespace {
constexpr auto JSONData = R"JSON(
    {
        "dos_guard": {
            "max_fetches": 100,
            "sweep_interval": 1,
            "max_connections": 2,
            "max_requests": 3,
            "whitelist": [
                "127.0.0.1"
            ]
        }
    }
)JSON";

constexpr auto IP = "127.0.0.2";

struct MockWhitelistHandler {
    MOCK_METHOD(bool, isWhiteListed, (std::string_view ip), (const));
};

using MockWhitelistHandlerType = NiceMock<MockWhitelistHandler>;

class FakeSweepHandler {
private:
    using guardType = BasicDOSGuard<MockWhitelistHandlerType, FakeSweepHandler>;
    guardType* dosGuard_;

public:
    void
    setup(guardType* guard)
    {
        dosGuard_ = guard;
    }

    void
    sweep()
    {
        dosGuard_->clear();
    }
};
};  // namespace

class DOSGuardTest : public NoLoggerFixture {
protected:
    Config cfg{json::parse(JSONData)};
    FakeSweepHandler sweepHandler{};
    MockWhitelistHandlerType whitelistHandler;
    BasicDOSGuard<MockWhitelistHandlerType, FakeSweepHandler> guard{cfg, whitelistHandler, sweepHandler};
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
    EXPECT_TRUE(guard.isOk(IP));
    guard.increment(IP);  // one connection
    EXPECT_TRUE(guard.isOk(IP));
    guard.increment(IP);  // two connections
    EXPECT_TRUE(guard.isOk(IP));
    guard.increment(IP);  // > two connections, can't connect more
    EXPECT_FALSE(guard.isOk(IP));

    guard.decrement(IP);
    EXPECT_TRUE(guard.isOk(IP));  // can connect again
}

TEST_F(DOSGuardTest, FetchCount)
{
    EXPECT_TRUE(guard.add(IP, 50));  // half of allowence
    EXPECT_TRUE(guard.add(IP, 50));  // now fully charged
    EXPECT_FALSE(guard.add(IP, 1));  // can't add even 1 anymore
    EXPECT_FALSE(guard.isOk(IP));

    guard.clear();                // force clear the above fetch count
    EXPECT_TRUE(guard.isOk(IP));  // can fetch again
}

TEST_F(DOSGuardTest, ClearFetchCountOnTimer)
{
    EXPECT_TRUE(guard.add(IP, 50));  // half of allowence
    EXPECT_TRUE(guard.add(IP, 50));  // now fully charged
    EXPECT_FALSE(guard.add(IP, 1));  // can't add even 1 anymore
    EXPECT_FALSE(guard.isOk(IP));

    sweepHandler.sweep();         // pretend sweep called from timer
    EXPECT_TRUE(guard.isOk(IP));  // can fetch again
}

TEST_F(DOSGuardTest, RequestLimit)
{
    EXPECT_TRUE(guard.request(IP));
    EXPECT_TRUE(guard.request(IP));
    EXPECT_TRUE(guard.request(IP));
    EXPECT_TRUE(guard.isOk(IP));
    EXPECT_FALSE(guard.request(IP));
    EXPECT_FALSE(guard.isOk(IP));
    guard.clear();
    EXPECT_TRUE(guard.isOk(IP));  // can request again
}

TEST_F(DOSGuardTest, RequestLimitOnTimer)
{
    EXPECT_TRUE(guard.request(IP));
    EXPECT_TRUE(guard.request(IP));
    EXPECT_TRUE(guard.request(IP));
    EXPECT_TRUE(guard.isOk(IP));
    EXPECT_FALSE(guard.request(IP));
    EXPECT_FALSE(guard.isOk(IP));
    sweepHandler.sweep();
    EXPECT_TRUE(guard.isOk(IP));  // can request again
}
