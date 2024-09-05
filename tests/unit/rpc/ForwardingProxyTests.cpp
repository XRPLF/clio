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

#include "rpc/Errors.hpp"
#include "rpc/common/impl/ForwardingProxy.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockCounters.hpp"
#include "util/MockHandlerProvider.hpp"
#include "util/MockLoadBalancer.hpp"
#include "util/Taggable.hpp"
#include "util/config/Config.hpp"
#include "web/Context.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <variant>

using namespace rpc;
using namespace testing;
namespace json = boost::json;

constexpr static auto CLIENT_IP = "127.0.0.1";

class RPCForwardingProxyTest : public HandlerBaseTest {
protected:
    std::shared_ptr<MockLoadBalancer> loadBalancer = std::make_shared<MockLoadBalancer>();
    std::shared_ptr<MockHandlerProvider> handlerProvider = std::make_shared<MockHandlerProvider>();
    MockCounters counters;

    util::Config config;
    util::TagDecoratorFactory tagFactory{config};

    rpc::impl::ForwardingProxy<MockLoadBalancer, MockCounters, MockHandlerProvider> proxy{
        loadBalancer,
        counters,
        handlerProvider
    };
};

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsFalseIfClioOnly)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const apiVersion = 2u;
    auto const method = "test";
    auto const params = json::parse("{}");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(true));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfProxied)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const apiVersion = 2u;
    auto const method = "submit";
    auto const params = json::parse("{}");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfCurrentLedgerSpecified)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const apiVersion = 2u;
    auto const method = "anymethod";
    auto const params = json::parse(R"({"ledger_index": "current"})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfClosedLedgerSpecified)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const apiVersion = 2u;
    auto const method = "anymethod";
    auto const params = json::parse(R"({"ledger_index": "closed"})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfAccountInfoWithQueueSpecified)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const apiVersion = 2u;
    auto const method = "account_info";
    auto const params = json::parse(R"({"queue": true})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsFalseIfAccountInfoQueueIsFalse)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const apiVersion = 2u;
    auto const method = "account_info";
    auto const params = json::parse(R"({"queue": false})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsTrueIfLedgerWithQueueSpecified)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const apiVersion = 2u;
    auto const method = "ledger";
    auto const params = json::parse(R"({"queue": true})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_TRUE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsFalseIfLedgerQueueIsFalse)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const apiVersion = 2u;
    auto const method = "ledger";
    auto const params = json::parse(R"({"queue": false})");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldNotForwardReturnsTrueIfAPIVersionIsV1)
{
    auto const apiVersion = 1u;
    auto const method = "api_version_check";
    auto const params = json::parse("{}");

    auto const rawHandlerProviderPtr = handlerProvider.get();
    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldForwardReturnsFalseIfAPIVersionIsV2)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const apiVersion = 2u;
    auto const method = "api_version_check";
    auto const params = json::parse("{}");

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(false));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(1);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldNeverForwardSubscribe)
{
    auto const apiVersion = 1u;
    auto const method = "subscribe";
    auto const params = json::parse("{}");

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ShouldNeverForwardUnsubscribe)
{
    auto const apiVersion = 1u;
    auto const method = "unsubscribe";
    auto const params = json::parse("{}");

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.shouldForward(ctx);
        ASSERT_FALSE(res);
    });
}

TEST_F(RPCForwardingProxyTest, ForwardCallsBalancerWithCorrectParams)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const rawBalancerPtr = loadBalancer.get();
    auto const apiVersion = 2u;
    auto const method = "submit";
    auto const params = json::parse(R"({"test": true})");
    auto const forwarded = json::parse(R"({"test": true, "command": "submit"})");

    EXPECT_CALL(
        *rawBalancerPtr, forwardToRippled(forwarded.as_object(), std::make_optional<std::string>(CLIENT_IP), true, _)
    )
        .WillOnce(Return(json::object{}));

    EXPECT_CALL(*rawHandlerProviderPtr, contains(method)).WillOnce(Return(true));

    EXPECT_CALL(counters, rpcForwarded(method));

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.forward(ctx);

        auto const data = std::get_if<json::object>(&res.response);
        EXPECT_TRUE(data != nullptr);
    });
}

TEST_F(RPCForwardingProxyTest, ForwardingFailYieldsErrorStatus)
{
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const rawBalancerPtr = loadBalancer.get();
    auto const apiVersion = 2u;
    auto const method = "submit";
    auto const params = json::parse(R"({"test": true})");
    auto const forwarded = json::parse(R"({"test": true, "command": "submit"})");

    EXPECT_CALL(
        *rawBalancerPtr, forwardToRippled(forwarded.as_object(), std::make_optional<std::string>(CLIENT_IP), true, _)
    )
        .WillOnce(Return(std::unexpected{rpc::ClioError::etlINVALID_RESPONSE}));

    EXPECT_CALL(*rawHandlerProviderPtr, contains(method)).WillOnce(Return(true));

    EXPECT_CALL(counters, rpcFailedToForward(method));

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx =
            web::Context(yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, true);

        auto const res = proxy.forward(ctx);

        auto const status = std::get_if<Status>(&res.response);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, rpc::ClioError::etlINVALID_RESPONSE);
    });
}
