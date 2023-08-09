//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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
#include <util/MockCounters.h>
#include <util/MockHandlerProvider.h>
#include <util/MockLoadBalancer.h>

#include <rpc/common/impl/ForwardingProxy.h>
#include <util/config/Config.h>

#include <boost/json.hpp>
#include <gtest/gtest.h>

using namespace RPC;
using namespace testing;

constexpr static auto CLIENT_IP = "127.0.0.1";

class RPCForwardingProxyTest : public HandlerBaseTest
{
protected:
    std::shared_ptr<MockLoadBalancer> loadBalancer = std::make_shared<MockLoadBalancer>();
    std::shared_ptr<MockHandlerProvider> handlerProvider = std::make_shared<MockHandlerProvider>();
    MockCounters counters;

    clio::util::Config config;
    clio::util::TagDecoratorFactory tagFactory{config};

    RPC::detail::ForwardingProxy<MockLoadBalancer, MockCounters, MockHandlerProvider> proxy{
        loadBalancer,
        counters,
        handlerProvider};
};

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsFalseIfClioOnly)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "test";
    auto const params = boost::json::parse("{}");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(true));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfProxied)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "submit";
    auto const params = boost::json::parse("{}");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfCurrentLedgerSpecified)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "anymethod";
    auto const params = boost::json::parse(R"({"ledger_index": "current"})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfClosedLedgerSpecified)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "anymethod";
    auto const params = boost::json::parse(R"({"ledger_index": "closed"})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfAccountInfoWithQueueSpecified)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "account_info";
    auto const params = boost::json::parse(R"({"queue": true})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfLedgerWithQueueSpecified)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "ledger";
    auto const params = boost::json::parse(R"({"queue": true})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfLedgerWithFullSpecified)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "ledger";
    auto const params = boost::json::parse(R"({"full": true})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfLedgerWithAccountsSpecified)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "ledger";
    auto const params = boost::json::parse(R"({"accounts": true})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsFalseIfAccountInfoQueueIsFalse)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "account_info";
    auto const params = boost::json::parse(R"({"queue": false})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsFalseIfLedgerQueueIsFalse)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "ledger";
    auto const params = boost::json::parse(R"({"queue": false})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsFalseIfLedgerFullIsFalse)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "ledger";
    auto const params = boost::json::parse(R"({"full": false})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsFalseIfLedgerAccountsIsFalse)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "ledger";
    auto const params = boost::json::parse(R"({"accounts": false})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfAPIVersionIsV1)
{
    auto const apiVersion = 1u;
    auto const method = "api_version_check";
    auto const params = boost::json::parse("{}");

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsFalseIfAPIVersionIsV2)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const apiVersion = 2u;
    auto const method = "api_version_check";
    auto const params = boost::json::parse("{}");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldNeverForwardSubscribe)
{
    auto const apiVersion = 1u;
    auto const method = "subscribe";
    auto const params = boost::json::parse("{}");

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldNeverForwardUnsubscribe)
{
    auto const apiVersion = 1u;
    auto const method = "unsubscribe";
    auto const params = boost::json::parse("{}");

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ForwardCallsBalancerWithCorrectParams)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const rawBalancerPtr = static_cast<MockLoadBalancer*>(loadBalancer.get());
    auto const apiVersion = 2u;
    auto const method = "submit";
    auto const params = boost::json::parse(R"({"test": true})");
    auto const forwarded = boost::json::parse(R"({"test": true, "command": "submit"})");

    ON_CALL(*rawBalancerPtr, forwardToRippled).WillByDefault(Return(std::make_optional<boost::json::object>()));
    EXPECT_CALL(*rawBalancerPtr, forwardToRippled(forwarded.as_object(), CLIENT_IP, _)).Times(1);

    ON_CALL(*rawHandlerProviderPtr, contains).WillByDefault(Return(true));
    EXPECT_CALL(*rawHandlerProviderPtr, contains(method)).Times(1);

    ON_CALL(counters, rpcForwarded).WillByDefault(Return());
    EXPECT_CALL(counters, rpcForwarded(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.forward(ctx);

        auto const data = std::get_if<boost::json::object>(&res);
        EXPECT_TRUE(data != nullptr);
    });
}

TEST_F(RPCForwardingProxyTest, ForwardingFailYieldsErrorStatus)
{
    auto const rawHandlerProviderPtr = static_cast<MockHandlerProvider*>(handlerProvider.get());
    auto const rawBalancerPtr = static_cast<MockLoadBalancer*>(loadBalancer.get());
    auto const apiVersion = 2u;
    auto const method = "submit";
    auto const params = boost::json::parse(R"({"test": true})");
    auto const forwarded = boost::json::parse(R"({"test": true, "command": "submit"})");

    ON_CALL(*rawBalancerPtr, forwardToRippled).WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*rawBalancerPtr, forwardToRippled(forwarded.as_object(), CLIENT_IP, _)).Times(1);

    ON_CALL(*rawHandlerProviderPtr, contains).WillByDefault(Return(true));
    EXPECT_CALL(*rawHandlerProviderPtr, contains(method)).Times(1);

    ON_CALL(counters, rpcFailedToForward).WillByDefault(Return());
    EXPECT_CALL(counters, rpcFailedToForward(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = mockBackendPtr->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP);

        auto const res = proxy.forward(ctx);

        auto const status = std::get_if<Status>(&res);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcFAILED_TO_FORWARD);
    });
}
