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
#include "util/NameGenerator.hpp"
#include "util/Taggable.hpp"
#include "util/config/Config.hpp"
#include "web/Context.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

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

struct ShouldForwardParamTestCaseBundle {
    std::string testName;
    std::uint32_t apiVersion;
    std::string method;
    std::string testJson;
    bool mockedIsClioOnly;
    std::uint32_t called;
    bool isAdmin;
    bool expected;
};

struct ShouldForwardParameterTest : public RPCForwardingProxyTest,
                                    WithParamInterface<ShouldForwardParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    auto const isClioOnly = true;
    auto const isAdmin = true;
    auto const shouldForward = true;

    return std::vector<ShouldForwardParamTestCaseBundle>{
        {.testName = "ShouldForwardReturnsFalseIfClioOnly",
         .apiVersion = 2u,
         .method = "test",
         .testJson = "{}",
         .mockedIsClioOnly = isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = !shouldForward},
        {.testName = "ShouldForwardReturnsTrueIfProxied",
         .apiVersion = 2u,
         .method = "submit",
         .testJson = "{}",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsTrueIfCurrentLedgerSpecified",
         .apiVersion = 2u,
         .method = "anymethod",
         .testJson = R"({"ledger_index": "current"})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsTrueIfClosedLedgerSpecified",
         .apiVersion = 2u,
         .method = "anymethod",
         .testJson = R"({"ledger_index": "closed"})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsTrueIfAccountInfoWithQueueSpecified",
         .apiVersion = 2u,
         .method = "account_info",
         .testJson = R"({"queue": true})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsFalseIfAccountInfoQueueIsFalse",
         .apiVersion = 2u,
         .method = "account_info",
         .testJson = R"({"queue": false})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = !shouldForward},
        {.testName = "ShouldForwardReturnsTrueIfLedgerWithQueueSpecified",
         .apiVersion = 2u,
         .method = "ledger",
         .testJson = R"({"queue": true})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsFalseIfLedgerQueueIsFalse",
         .apiVersion = 2u,
         .method = "ledger",
         .testJson = R"({"queue": false})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = !shouldForward},
        {.testName = "ShouldNotForwardReturnsTrueIfAPIVersionIsV1",
         .apiVersion = 1u,
         .method = "api_version_check",
         .testJson = "{}",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = !shouldForward},
        {.testName = "ShouldForwardReturnsFalseIfAPIVersionIsV2",
         .apiVersion = 2u,
         .method = "api_version_check",
         .testJson = "{}",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = !shouldForward},
        {.testName = "ShouldNeverForwardSubscribe",
         .apiVersion = 1u,
         .method = "subscribe",
         .testJson = "{}",
         .mockedIsClioOnly = !isClioOnly,
         .called = 0,
         .isAdmin = !isAdmin,
         .expected = !shouldForward},
        {.testName = "ShouldNeverForwardUnsubscribe",
         .apiVersion = 1u,
         .method = "unsubscribe",
         .testJson = "{}",
         .mockedIsClioOnly = !isClioOnly,
         .called = 0,
         .isAdmin = !isAdmin,
         .expected = !shouldForward},
        {.testName = "ForceForwardTrue",
         .apiVersion = 1u,
         .method = "any_method",
         .testJson = R"({"force_forward": true})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = isAdmin,
         .expected = shouldForward},
        {.testName = "ForceForwardFalse",
         .apiVersion = 1u,
         .method = "any_method",
         .testJson = R"({"force_forward": false})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = isAdmin,
         .expected = !shouldForward},
        {.testName = "ForceForwardNotAdmin",
         .apiVersion = 1u,
         .method = "any_method",
         .testJson = R"({"force_forward": true})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = !shouldForward},
        {.testName = "ForceForwardSubscribe",
         .apiVersion = 1u,
         .method = "subscribe",
         .testJson = R"({"force_forward": true})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 0,
         .isAdmin = isAdmin,
         .expected = not shouldForward},
        {.testName = "ForceForwardUnsubscribe",
         .apiVersion = 1u,
         .method = "unsubscribe",
         .testJson = R"({"force_forward": true})",
         .mockedIsClioOnly = !isClioOnly,
         .called = 0,
         .isAdmin = isAdmin,
         .expected = !shouldForward},
        {.testName = "ForceForwardClioOnly",
         .apiVersion = 1u,
         .method = "clio_only_method",
         .testJson = R"({"force_forward": true})",
         .mockedIsClioOnly = isClioOnly,
         .called = 1,
         .isAdmin = isAdmin,
         .expected = !shouldForward},
    };
}

INSTANTIATE_TEST_CASE_P(
    ShouldForwardTest,
    ShouldForwardParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(ShouldForwardParameterTest, Test)
{
    auto const testBundle = GetParam();
    auto const rawHandlerProviderPtr = handlerProvider.get();
    auto const apiVersion = testBundle.apiVersion;
    auto const method = testBundle.method;
    auto const params = json::parse(testBundle.testJson);

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_)).WillByDefault(Return(testBundle.mockedIsClioOnly));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(testBundle.called);

    runSpawn([&](auto yield) {
        auto const range = backend->fetchLedgerRange();
        auto const ctx = web::Context(
            yield, method, apiVersion, params.as_object(), nullptr, tagFactory, *range, CLIENT_IP, testBundle.isAdmin
        );

        auto const res = proxy.shouldForward(ctx);
        ASSERT_EQ(res, testBundle.expected);
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
