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

#include <util/Fixtures.h>

#include <config/Config.h>
#include <webserver/DOSGuard.h>

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>

using namespace testing;
using namespace clio;
using namespace std;
namespace json = boost::json;

namespace {
constexpr static auto JSONData = R"JSON(
    {
        "dos_guard": {
            "max_fetches": 100,
            "sweep_interval": 1,
            "max_connections": 2,
            "max_requests": 3,
            "whitelist": [
                "127.0.0.1",
                "192.168.0.1/22", 
                "10.0.0.1",
                "2001:0db8:85a3:0000:0000:8a2e:0000:0000/22"
            ]
        }
    }
)JSON";

constexpr static auto JSONData2 = R"JSON(
    {
        "dos_guard": {
            "max_fetches": 100,
            "sweep_interval": 0.1,
            "max_connections": 2,
            "whitelist": ["127.0.0.1"]
        }
    }
)JSON";

constexpr static auto IP = "127.0.0.2";

class FakeSweepHandler
{
private:
    using guard_type = BasicDOSGuard<WhitelistHandler, FakeSweepHandler>;
    guard_type* dosGuard_;

public:
    void
    setup(guard_type* guard)
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

class DOSGuardTest : public NoLoggerFixture
{
protected:
    Config cfg{json::parse(JSONData)};
    FakeSweepHandler sweepHandler;
    BasicDOSGuard<WhitelistHandler, FakeSweepHandler> guard{cfg, sweepHandler};
};

TEST_F(DOSGuardTest, Whitelisting)
{
    EXPECT_TRUE(guard.isWhiteListed("127.0.0.1"));
    EXPECT_FALSE(guard.isWhiteListed(IP));
}

TEST_F(DOSGuardTest, CIDRWhitelistTest)
{
    EXPECT_TRUE(guard.isWhiteListed("192.168.1.10"));
    EXPECT_FALSE(guard.isWhiteListed("193.168.0.123"));
    EXPECT_TRUE(guard.isWhiteListed("10.0.0.1"));
    EXPECT_FALSE(guard.isWhiteListed("10.0.0.2"));
    EXPECT_TRUE(guard.isWhiteListed("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
    EXPECT_FALSE(guard.isWhiteListed("2002:1db8:85a3:0000:0000:8a2e:0370:7334"));
    EXPECT_FALSE(guard.isWhiteListed(IP));
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

template <typename SweepHandler>
struct BasicDOSGuardMock : public BaseDOSGuard
{
    BasicDOSGuardMock(SweepHandler& handler)
    {
        handler.setup(this);
    }

    MOCK_METHOD(void, clear, (), (noexcept, override));
};

class DOSGuardIntervalSweepHandlerTest : public SyncAsioContextTest
{
protected:
    Config cfg{json::parse(JSONData2)};
    IntervalSweepHandler sweepHandler{cfg, ctx};
    BasicDOSGuardMock<IntervalSweepHandler> guard{sweepHandler};
};

TEST_F(DOSGuardIntervalSweepHandlerTest, SweepAfterInterval)
{
    EXPECT_CALL(guard, clear()).Times(AtLeast(2));
    ctx.run_for(std::chrono::milliseconds(400));
}
