#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/impl/ForwardingProxy.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockCounters.hpp"
#include "util/MockHandlerProvider.hpp"
#include "util/MockLoadBalancer.hpp"
#include "util/NameGenerator.hpp"
#include "util/Taggable.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "web/Context.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
using namespace testing;
using namespace util::config;

namespace {
constexpr auto kClientIp = "127.0.0.1";
}  // namespace

class RPCForwardingProxyTest : public HandlerBaseTest {
protected:
    std::shared_ptr<MockLoadBalancer> loadBalancer_ = std::make_shared<MockLoadBalancer>();
    std::shared_ptr<MockHandlerProvider> handlerProvider_ = std::make_shared<MockHandlerProvider>();
    MockCounters counters_;

    ClioConfigDefinition const config_{
        {"log.tag_style", ConfigValue{ConfigType::String}.defaultValue("none")}
    };
    util::TagDecoratorFactory tagFactory_{config_};

    rpc::impl::ForwardingProxy<MockCounters, MockHandlerProvider> proxy_{
        loadBalancer_,
        counters_,
        handlerProvider_
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
        {.testName = "ShouldForwardReturnsTrueIsNotAdminSimulate",
         .apiVersion = 1u,
         .method = "simulate",
         .testJson = "{}",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsTrueIsAdminSimulate",
         .apiVersion = 1u,
         .method = "simulate",
         .testJson = "{}",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsTrueIsNotAdminRipplePathFind",
         .apiVersion = 2u,
         .method = "ripple_path_find",
         .testJson = R"JSON({"force_forward": true})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsTrueIsAdminRipplePathFind",
         .apiVersion = 2u,
         .method = "ripple_path_find",
         .testJson = R"JSON({"force_forward": true})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsTrueIfCurrentLedgerSpecified",
         .apiVersion = 2u,
         .method = "anymethod",
         .testJson = R"JSON({"ledger_index": "current"})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsTrueIfClosedLedgerSpecified",
         .apiVersion = 2u,
         .method = "anymethod",
         .testJson = R"JSON({"ledger_index": "closed"})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsTrueIfAccountInfoWithQueueSpecified",
         .apiVersion = 2u,
         .method = "account_info",
         .testJson = R"JSON({"queue": true})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsFalseIfAccountInfoQueueIsFalse",
         .apiVersion = 2u,
         .method = "account_info",
         .testJson = R"JSON({"queue": false})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = !shouldForward},
        {.testName = "ShouldForwardReturnsTrueIfLedgerWithQueueSpecified",
         .apiVersion = 2u,
         .method = "ledger",
         .testJson = R"JSON({"queue": true})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = shouldForward},
        {.testName = "ShouldForwardReturnsFalseIfLedgerQueueIsFalse",
         .apiVersion = 2u,
         .method = "ledger",
         .testJson = R"JSON({"queue": false})JSON",
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
         .testJson = R"JSON({"force_forward": true})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = isAdmin,
         .expected = shouldForward},
        {.testName = "ForceForwardFalse",
         .apiVersion = 1u,
         .method = "any_method",
         .testJson = R"JSON({"force_forward": false})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = isAdmin,
         .expected = !shouldForward},
        {.testName = "ForceForwardNotAdmin",
         .apiVersion = 1u,
         .method = "any_method",
         .testJson = R"JSON({"force_forward": true})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 1,
         .isAdmin = !isAdmin,
         .expected = !shouldForward},
        {.testName = "ForceForwardSubscribe",
         .apiVersion = 1u,
         .method = "subscribe",
         .testJson = R"JSON({"force_forward": true})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 0,
         .isAdmin = isAdmin,
         .expected = not shouldForward},
        {.testName = "ForceForwardUnsubscribe",
         .apiVersion = 1u,
         .method = "unsubscribe",
         .testJson = R"JSON({"force_forward": true})JSON",
         .mockedIsClioOnly = !isClioOnly,
         .called = 0,
         .isAdmin = isAdmin,
         .expected = !shouldForward},
        {.testName = "ForceForwardClioOnly",
         .apiVersion = 1u,
         .method = "clio_only_method",
         .testJson = R"JSON({"force_forward": true})JSON",
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
    tests::util::kNameGenerator
);

TEST_P(ShouldForwardParameterTest, Test)
{
    auto const testBundle = GetParam();
    auto const rawHandlerProviderPtr = handlerProvider_.get();
    auto const apiVersion = testBundle.apiVersion;
    auto const method = testBundle.method;
    auto const params = boost::json::parse(testBundle.testJson);

    ON_CALL(*rawHandlerProviderPtr, isClioOnly(_))
        .WillByDefault(Return(testBundle.mockedIsClioOnly));
    EXPECT_CALL(*rawHandlerProviderPtr, isClioOnly(method)).Times(testBundle.called);

    runSpawn([&](auto yield) {
        auto const ctx = web::Context{
            yield,
            method,
            apiVersion,
            params.as_object(),
            nullptr,
            tagFactory_,
            data::LedgerRange{},
            kClientIp,
            testBundle.isAdmin,
        };

        auto const res = proxy_.shouldForward(ctx);
        ASSERT_EQ(res, testBundle.expected);
    });
}

TEST_F(RPCForwardingProxyTest, ForwardCallsBalancerWithCorrectParams)
{
    auto const rawHandlerProviderPtr = handlerProvider_.get();
    auto const rawBalancerPtr = loadBalancer_.get();
    auto const apiVersion = 2u;
    auto const method = "submit";
    auto const params = boost::json::parse(R"JSON({"test": true})JSON");
    auto const forwarded = boost::json::parse(R"JSON({"test": true, "command": "submit"})JSON");

    EXPECT_CALL(
        *rawBalancerPtr,
        forwardToRippled(forwarded.as_object(), std::make_optional<std::string>(kClientIp), true, _)
    )
        .WillOnce(Return(boost::json::object{}));

    EXPECT_CALL(*rawHandlerProviderPtr, contains(method)).WillOnce(Return(true));

    EXPECT_CALL(counters_, rpcForwarded(method));

    runSpawn([&](auto yield) {
        auto const ctx = web::Context{
            yield,
            method,
            apiVersion,
            params.as_object(),
            nullptr,
            tagFactory_,
            data::LedgerRange{},
            kClientIp,
            true,
        };

        auto const res = proxy_.forward(ctx);

        EXPECT_TRUE(res.response.has_value());
    });
}

TEST_F(RPCForwardingProxyTest, ForwardingFailYieldsErrorStatus)
{
    auto const rawHandlerProviderPtr = handlerProvider_.get();
    auto const rawBalancerPtr = loadBalancer_.get();
    auto const apiVersion = 2u;
    auto const method = "submit";
    auto const params = boost::json::parse(R"JSON({"test": true})JSON");
    auto const forwarded = boost::json::parse(R"JSON({"test": true, "command": "submit"})JSON");

    EXPECT_CALL(
        *rawBalancerPtr,
        forwardToRippled(forwarded.as_object(), std::make_optional<std::string>(kClientIp), true, _)
    )
        .WillOnce(Return(std::unexpected{rpc::ClioError::EtlInvalidResponse}));

    EXPECT_CALL(*rawHandlerProviderPtr, contains(method)).WillOnce(Return(true));

    EXPECT_CALL(counters_, rpcFailedToForward(method));

    runSpawn([&](auto yield) {
        auto const ctx = web::Context{
            yield,
            method,
            apiVersion,
            params.as_object(),
            nullptr,
            tagFactory_,
            data::LedgerRange{},
            kClientIp,
            true,
        };

        auto const res = proxy_.forward(ctx);

        EXPECT_FALSE(res.response.has_value());
        EXPECT_EQ(res.response.error(), rpc::ClioError::EtlInvalidResponse);
    });
}
