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
            "whitelist": ["127.0.0.1"]
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

// Note: Can have a mixed bag whitelist like the below
// Whitelist contains raw IP not in any subnet, raw IP in a subnet, a subnet
// viable with no raw IPs in whitelist, a subnet viable with a raw IP in a
// whitelist
constexpr static auto JSONData3 = R"JSON(
    {
        "dos_guard": {
            "max_fetches": 100,
            "sweep_interval": 1,
            "max_connections": 2,
            "max_requests": 3,
            "whitelist": ["198.54.8.17", "127.0.0.1/24", "10.3.255.254/14", "21DA:00D4:0000:2F4C:02BC:00FF:FE18:4C5A", "2001:0:eab:DEAD:0:A0:ABCD:4E/64"]
        }
    }
)JSON";

constexpr static auto IP = "127.0.0.2";

class FakeSweepHandler
{
private:
    using guard_type = BasicDOSGuard<FakeSweepHandler>;
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
    Config cfg_static_list{json::parse(JSONData3)};
    FakeSweepHandler sweepHandler;
    FakeSweepHandler sweepHandler_static_list_ipv4;
    BasicDOSGuard<FakeSweepHandler> guard{cfg, sweepHandler};
    BasicDOSGuard<FakeSweepHandler> guard_mixed{
        cfg_static_list,
        sweepHandler_static_list_ipv4};
};

TEST_F(DOSGuardTest, Whitelisting)
{
    // Test cases for individual IPv4 address
    EXPECT_TRUE(guard.isWhiteListed("127.0.0.1"));
    EXPECT_FALSE(guard.isWhiteListed(IP));

    // Test cases for list of IPv4 addresses
    // Checks if addresses not explicitly listed aren't there
    EXPECT_TRUE(guard_mixed.isWhiteListed("198.54.8.17"));
    EXPECT_TRUE(
        guard_mixed.isWhiteListed("21DA:00D4:0000:2F4C:02BC:00FF:FE18:4C5A"));
    EXPECT_FALSE(guard_mixed.isWhiteListed("127.0.1.0"));
    EXPECT_FALSE(
        guard_mixed.isWhiteListed("DEAD:00D4:0000:2F4C:02BC:00FF:FE18:4C5A"));

    // Test to see if whitelist checks against subnets (in vs out of subnet)
    EXPECT_TRUE(guard_mixed.isWhiteListed("127.0.0.1"));
    EXPECT_TRUE(guard_mixed.isWhiteListed("127.0.0.2"));
    EXPECT_TRUE(guard_mixed.isWhiteListed("10.3.255.254"));
    EXPECT_TRUE(
        guard_mixed.isWhiteListed("2001:0000:0EAB:DEAD:0000:00A0:ABCD:AAAA"));
    EXPECT_TRUE(
        guard_mixed.isWhiteListed("2001:0000:0EAB:DEAD:0000:00A0:ABCD:004E"));
    EXPECT_TRUE(
        guard_mixed.isWhiteListed("2001:0000:0EAB:DEAD:FFFF:FFFF:FFFF:FFFF"));
    EXPECT_FALSE(guard_mixed.isWhiteListed("10.4.0.0"));
    EXPECT_FALSE(
        guard_mixed.isWhiteListed("2001:0000:DEAD:DEAD:FFFF:FFFF:FFFF:FFFF"));

    // Check against reserved IP addresses within subnets
    EXPECT_FALSE(guard_mixed.isWhiteListed("127.0.0.0"));
    EXPECT_FALSE(guard_mixed.isWhiteListed("127.0.0.255"));
    EXPECT_FALSE(guard_mixed.isWhiteListed("10.0.0.0"));
    EXPECT_FALSE(guard_mixed.isWhiteListed("10.3.255.255"));
    EXPECT_FALSE(
        guard_mixed.isWhiteListed("0000:0000:0EAB:DEAD:FFFF:FFFF:FFFF:FFFF"));

    // Check that CIDR notation is not allowed
    EXPECT_FALSE(guard_mixed.isWhiteListed("10.3.255.254/14"));
    EXPECT_FALSE(guard_mixed.isWhiteListed("10.3.255.254/16"));
    EXPECT_FALSE(guard_mixed.isWhiteListed("127.0.0.1/24"));
    EXPECT_FALSE(guard_mixed.isWhiteListed("2001:0:eab:DEAD:0:A0:ABCD:4E/64"));
}

TEST_F(DOSGuardTest, checkIfIPNotMalformed)
{
    // IPv4 test cases
    EXPECT_TRUE(guard_mixed.checkValidityOfWhitelist("10.3.255.254"));
    EXPECT_TRUE(guard_mixed.checkValidityOfWhitelist("10.3.255.254/14"));
    EXPECT_FALSE(guard_mixed.checkValidityOfWhitelist("10.3.255.-1"));
    EXPECT_FALSE(guard_mixed.checkValidityOfWhitelist("10.3.255.-1/14"));

    // IPv6 test cases
    EXPECT_TRUE(
        guard_mixed.checkValidityOfWhitelist("2001:0:eab:DEAD:0:A0:ABCD:4E"));
    EXPECT_TRUE(
        guard_mixed.checkValidityOfWhitelist("2001:0:0eab:dead::a0:abcd:4e"));
    EXPECT_TRUE(guard_mixed.checkValidityOfWhitelist(
        "21DA:00D4:0000:2F4C:02BC:00FF:FE18:4C5A/64"));
    EXPECT_TRUE(guard_mixed.checkValidityOfWhitelist(
        "21DA:00D4::2F4C:02BC:00FF:FE18:4C5A/64"));
    EXPECT_FALSE(
        guard_mixed.checkValidityOfWhitelist("2001::eab:dead::a0:abcd:-1"));
    EXPECT_FALSE(guard_mixed.checkValidityOfWhitelist(
        "21DA:00D4:0000:2F4C:02BC:00FF:FE18:-1/64"));
}

TEST_F(DOSGuardTest, checkIfInSubnet)
{
    // IPv4 Test Cases
    EXPECT_TRUE(
        guard_mixed.isIPv4AddressInSubnet("10.3.255.254", "10.0.0.0/14"));
    EXPECT_FALSE(guard_mixed.isIPv4AddressInSubnet("10.4.0.0", "10.0.0.0/14"));

    EXPECT_TRUE(
        guard_mixed.isIPv4AddressInSubnet("192.168.0.1", "192.168.0.0/16"));
    EXPECT_FALSE(
        guard_mixed.isIPv4AddressInSubnet("192.169.0.1", "192.168.0.0/16"));

    // IPv6 Test Cases
    EXPECT_TRUE(guard_mixed.isIPv6AddressInSubnet(
        "21DA:00D4:0000:2F4C:02BC:00FF:FE18:4C5A", "21DA:00D4:0000:2F4C::/64"));
    EXPECT_FALSE(guard_mixed.isIPv6AddressInSubnet(
        "21DA:00D4:0000:2F4D:02BC:00FF:FE18:4C5A", "21DA:00D4:0000:2F4C::/64"));
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
    ctx.run_for(std::chrono::milliseconds(300));
}
