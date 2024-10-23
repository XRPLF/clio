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

#include "data/BackendInterface.hpp"
#include "rpc/Errors.hpp"
#include "rpc/FakesAndMocks.hpp"
#include "rpc/RPCEngine.hpp"
#include "rpc/WorkQueue.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockCounters.hpp"
#include "util/MockCountersFixture.hpp"
#include "util/MockETLServiceTestFixture.hpp"
#include "util/MockHandlerProvider.hpp"
#include "util/MockLoadBalancer.hpp"
#include "util/MockPrometheus.hpp"
#include "util/NameGenerator.hpp"
#include "util/Taggable.hpp"
#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/dosguard/DOSGuard.hpp"
#include "web/dosguard/WhitelistHandler.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using namespace rpc;
using namespace util;
namespace json = boost::json;
using namespace testing;
using namespace util::config;

namespace {
constexpr auto FORWARD_REPLY = R"JSON({
    "result": 
    {
        "status": "success",
        "forwarded": true
    }
})JSON";
}  // namespace

inline ClioConfigDefinition
generateDefaultRPCEngineConfig()
{
    return ClioConfigDefinition{
        {"server.max_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(2)},
        {"workers", ConfigValue{ConfigType::Integer}.defaultValue(4).withConstraint(validateUint16)},
        {"rpc.cache_timeout", ConfigValue{ConfigType::Double}.defaultValue(0.0).withConstraint(validatePositiveDouble)},
        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")},
        {"dos_guard.whitelist.[]", Array{ConfigValue{ConfigType::String}.optional()}},
        {"dos_guard.max_fetches",
         ConfigValue{ConfigType::Integer}.defaultValue(1000'000u).withConstraint(validateUint32)},
        {"dos_guard.max_connections", ConfigValue{ConfigType::Integer}.defaultValue(20u).withConstraint(validateUint32)
        },
        {"dos_guard.max_requests", ConfigValue{ConfigType::Integer}.defaultValue(20u).withConstraint(validateUint32)}
    };
}

struct RPCEngineTest : util::prometheus::WithPrometheus,
                       MockBackendTest,
                       MockCountersTest,
                       MockLoadBalancerTest,
                       SyncAsioContextTest {
    ClioConfigDefinition cfg = generateDefaultRPCEngineConfig();

    util::TagDecoratorFactory tagFactory{cfg};
    WorkQueue queue = WorkQueue::make_WorkQueue(cfg);
    web::dosguard::WhitelistHandler whitelistHandler{cfg};
    web::dosguard::DOSGuard dosGuard{cfg, whitelistHandler};
    std::shared_ptr<MockHandlerProvider> handlerProvider = std::make_shared<MockHandlerProvider>();
};

struct RPCEngineFlowTestCaseBundle {
    std::string testName;
    bool isAdmin;
    std::string method;
    std::string params;
    bool forwarded;
    std::optional<bool> isTooBusy;
    std::optional<bool> isUnknownCmd;
    bool handlerReturnError;
    std::optional<rpc::Status> status;
    std::optional<boost::json::object> response;
};

struct RPCEngineFlowParameterTest : public RPCEngineTest, WithParamInterface<RPCEngineFlowTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    auto const neverCalled = std::nullopt;

    return std::vector<RPCEngineFlowTestCaseBundle>{
        {.testName = "ForwardedSubmit",
         .isAdmin = true,
         .method = "submit",
         .params = "{}",
         .forwarded = true,
         .isTooBusy = neverCalled,
         .isUnknownCmd = neverCalled,
         .handlerReturnError = false,
         .status = rpc::Status{},
         .response = boost::json::parse(FORWARD_REPLY).as_object()},
        {.testName = "ForwardAdminCmd",
         .isAdmin = false,
         .method = "ledger",
         .params = R"JSON({"full": true, "ledger_index": "current"})JSON",
         .forwarded = false,
         .isTooBusy = neverCalled,
         .isUnknownCmd = neverCalled,
         .handlerReturnError = false,
         .status = rpc::Status{RippledError::rpcNO_PERMISSION},
         .response = std::nullopt},
        {.testName = "BackendTooBusy",
         .isAdmin = false,
         .method = "ledger",
         .params = "{}",
         .forwarded = false,
         .isTooBusy = true,
         .isUnknownCmd = neverCalled,
         .handlerReturnError = false,
         .status = rpc::Status{RippledError::rpcTOO_BUSY},
         .response = std::nullopt},
        {.testName = "HandlerUnknown",
         .isAdmin = false,
         .method = "ledger",
         .params = "{}",
         .forwarded = false,
         .isTooBusy = false,
         .isUnknownCmd = true,
         .handlerReturnError = false,
         .status = rpc::Status{RippledError::rpcUNKNOWN_COMMAND},
         .response = std::nullopt},
        {.testName = "HandlerReturnError",
         .isAdmin = false,
         .method = "ledger",
         .params = R"JSON({"hello": "world", "limit": 50})JSON",
         .forwarded = false,
         .isTooBusy = false,
         .isUnknownCmd = false,
         .handlerReturnError = true,
         .status = rpc::Status{"Very custom error"},
         .response = std::nullopt},
        {.testName = "HandlerReturnResponse",
         .isAdmin = false,
         .method = "ledger",
         .params = R"JSON({"hello": "world", "limit": 50})JSON",
         .forwarded = false,
         .isTooBusy = false,
         .isUnknownCmd = false,
         .handlerReturnError = false,
         .status = std::nullopt,
         .response = boost::json::parse(R"JSON({"computed": "world_50"})JSON").as_object()},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCEngineFlow,
    RPCEngineFlowParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(RPCEngineFlowParameterTest, Test)
{
    auto const& testBundle = GetParam();

    std::shared_ptr<RPCEngine<MockLoadBalancer, MockCounters>> engine =
        RPCEngine<MockLoadBalancer, MockCounters>::make_RPCEngine(
            generateDefaultRPCEngineConfig(),
            backend,
            mockLoadBalancerPtr,
            dosGuard,
            queue,
            *mockCountersPtr,
            handlerProvider
        );

    if (testBundle.forwarded) {
        EXPECT_CALL(*mockLoadBalancerPtr, forwardToRippled)
            .WillOnce(Return(std::expected<boost::json::object, rpc::ClioError>(json::parse(FORWARD_REPLY).as_object()))
            );
        EXPECT_CALL(*handlerProvider, contains).WillOnce(Return(true));
        EXPECT_CALL(*mockCountersPtr, rpcForwarded(testBundle.method));
    }

    if (testBundle.isTooBusy.has_value()) {
        EXPECT_CALL(*backend, isTooBusy).WillOnce(Return(*testBundle.isTooBusy));
        if (testBundle.isTooBusy.value())
            EXPECT_CALL(*mockCountersPtr, onTooBusy);
    }

    EXPECT_CALL(*handlerProvider, isClioOnly).WillOnce(Return(false));

    if (testBundle.isUnknownCmd.has_value()) {
        if (testBundle.isUnknownCmd.value()) {
            EXPECT_CALL(*handlerProvider, getHandler).WillOnce(Return(std::nullopt));
            EXPECT_CALL(*mockCountersPtr, onUnknownCommand);
        } else {
            if (testBundle.handlerReturnError) {
                EXPECT_CALL(*handlerProvider, getHandler)
                    .WillOnce(Return(AnyHandler{tests::common::FailingHandlerFake{}}));
                EXPECT_CALL(*mockCountersPtr, rpcErrored(testBundle.method));
                EXPECT_CALL(*handlerProvider, contains(testBundle.method)).WillOnce(Return(true));
            } else {
                EXPECT_CALL(*handlerProvider, getHandler(testBundle.method))
                    .WillOnce(Return(AnyHandler{tests::common::HandlerFake{}}));
            }
        }
    }

    runSpawn([&](auto yield) {
        auto const ctx = web::Context(
            yield,
            testBundle.method,
            1,  // api version
            boost::json::parse(testBundle.params).as_object(),
            nullptr,
            tagFactory,
            LedgerRange{0, 30},
            "127.0.0.2",
            testBundle.isAdmin
        );

        auto const res = engine->buildResponse(ctx);
        auto const status = std::get_if<rpc::Status>(&res.response);
        auto const response = std::get_if<boost::json::object>(&res.response);
        ASSERT_EQ(status == nullptr, testBundle.response.has_value());
        if (testBundle.response.has_value()) {
            EXPECT_EQ(*response, testBundle.response.value());
        } else {
            EXPECT_TRUE(*status == testBundle.status.value());
        }
    });
}

TEST_F(RPCEngineTest, ThrowDatabaseError)
{
    auto const method = "subscribe";
    std::shared_ptr<RPCEngine<MockLoadBalancer, MockCounters>> engine =
        RPCEngine<MockLoadBalancer, MockCounters>::make_RPCEngine(
            cfg, backend, mockLoadBalancerPtr, dosGuard, queue, *mockCountersPtr, handlerProvider
        );
    EXPECT_CALL(*backend, isTooBusy).WillOnce(Return(false));
    EXPECT_CALL(*handlerProvider, getHandler(method)).WillOnce(Return(AnyHandler{tests::common::FailingHandlerFake{}}));
    EXPECT_CALL(*mockCountersPtr, rpcErrored(method)).WillOnce(Throw(data::DatabaseTimeout{}));
    EXPECT_CALL(*handlerProvider, contains(method)).WillOnce(Return(true));
    EXPECT_CALL(*mockCountersPtr, onTooBusy());

    runSpawn([&](auto yield) {
        auto const ctx = web::Context(
            yield,
            method,
            1,
            boost::json::parse("{}").as_object(),
            nullptr,
            tagFactory,
            LedgerRange{0, 30},
            "127.0.0.2",
            false
        );

        auto const res = engine->buildResponse(ctx);
        auto const status = std::get_if<rpc::Status>(&res.response);
        ASSERT_TRUE(status != nullptr);
        EXPECT_TRUE(*status == Status{RippledError::rpcTOO_BUSY});
    });
}

TEST_F(RPCEngineTest, ThrowException)
{
    auto const method = "subscribe";
    std::shared_ptr<RPCEngine<MockLoadBalancer, MockCounters>> engine =
        RPCEngine<MockLoadBalancer, MockCounters>::make_RPCEngine(
            cfg, backend, mockLoadBalancerPtr, dosGuard, queue, *mockCountersPtr, handlerProvider
        );
    EXPECT_CALL(*backend, isTooBusy).WillOnce(Return(false));
    EXPECT_CALL(*handlerProvider, getHandler(method)).WillOnce(Return(AnyHandler{tests::common::FailingHandlerFake{}}));
    EXPECT_CALL(*mockCountersPtr, rpcErrored(method)).WillOnce(Throw(std::exception{}));
    EXPECT_CALL(*handlerProvider, contains(method)).WillOnce(Return(true));
    EXPECT_CALL(*mockCountersPtr, onInternalError());

    runSpawn([&](auto yield) {
        auto const ctx = web::Context(
            yield,
            method,
            1,
            boost::json::parse("{}").as_object(),
            nullptr,
            tagFactory,
            LedgerRange{0, 30},
            "127.0.0.2",
            false
        );

        auto const res = engine->buildResponse(ctx);
        auto const status = std::get_if<rpc::Status>(&res.response);
        ASSERT_TRUE(status != nullptr);
        EXPECT_TRUE(*status == Status{RippledError::rpcINTERNAL});
    });
}

struct RPCEngineCacheTestCaseBundle {
    std::string testName;
    std::string config;
    std::string method;
    bool isAdmin;
    bool expectedCacheEnabled;
};

struct RPCEngineCacheParameterTest : public RPCEngineTest, WithParamInterface<RPCEngineCacheTestCaseBundle> {};

static auto
generateCacheTestValuesForParametersTest()
{
    return std::vector<RPCEngineCacheTestCaseBundle>{
        {.testName = "CacheEnabled",
         .config = R"JSON({
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": 
            {"cache_timeout": 10}
         })JSON",
         .method = "server_info",
         .isAdmin = false,
         .expectedCacheEnabled = true},
        {.testName = "CacheDisabledWhenNoConfig",
         .config = R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": {"cache_timeout": 0}
         })JSON",
         .method = "server_info",
         .isAdmin = false,
         .expectedCacheEnabled = false},
        {.testName = "CacheDisabledWhenNoTimeout",
         .config = R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": {"cache_timeout": 0}
         })JSON",
         .method = "server_info",
         .isAdmin = false,
         .expectedCacheEnabled = false},
        {.testName = "CacheDisabledWhenTimeoutIsZero",
         .config = R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": {"cache_timeout": 0}
         })JSON",
         .method = "server_info",
         .isAdmin = false,
         .expectedCacheEnabled = false},
        {.testName = "CacheNotWorkForAdmin",
         .config = R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": { "cache_timeout": 10}
         })JSON",
         .method = "server_info",
         .isAdmin = true,
         .expectedCacheEnabled = false},
        {.testName = "CacheDisabledWhenCmdNotMatch",
         .config = R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": {"cache_timeout": 10}
         })JSON",
         .method = "server_info2",
         .isAdmin = false,
         .expectedCacheEnabled = false},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCEngineCache,
    RPCEngineCacheParameterTest,
    ValuesIn(generateCacheTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(RPCEngineCacheParameterTest, Test)
{
    auto const& testParam = GetParam();
    auto const json = ConfigFileJson{json::parse(testParam.config).as_object()};

    auto cfgCache{generateDefaultRPCEngineConfig()};
    auto const errors = cfgCache.parse(json);
    EXPECT_TRUE(!errors.has_value());

    auto const admin = testParam.isAdmin;
    auto const method = testParam.method;
    std::shared_ptr<RPCEngine<MockLoadBalancer, MockCounters>> engine =
        RPCEngine<MockLoadBalancer, MockCounters>::make_RPCEngine(
            cfgCache, backend, mockLoadBalancerPtr, dosGuard, queue, *mockCountersPtr, handlerProvider
        );
    int callTime = 2;
    EXPECT_CALL(*handlerProvider, isClioOnly).Times(callTime).WillRepeatedly(Return(false));
    if (testParam.expectedCacheEnabled) {
        EXPECT_CALL(*backend, isTooBusy).WillOnce(Return(false));
        EXPECT_CALL(*handlerProvider, getHandler).WillOnce(Return(AnyHandler{tests::common::HandlerFake{}}));

    } else {
        EXPECT_CALL(*backend, isTooBusy).Times(callTime).WillRepeatedly(Return(false));
        EXPECT_CALL(*handlerProvider, getHandler)
            .Times(callTime)
            .WillRepeatedly(Return(AnyHandler{tests::common::HandlerFake{}}));
    }

    while (callTime-- != 0) {
        runSpawn([&](auto yield) {
            auto const ctx = web::Context(
                yield,
                method,
                1,
                boost::json::parse(R"JSON({"hello": "world", "limit": 50})JSON").as_object(),
                nullptr,
                tagFactory,
                LedgerRange{0, 30},
                "127.0.0.2",
                admin
            );

            auto const res = engine->buildResponse(ctx);
            auto const response = std::get_if<boost::json::object>(&res.response);
            EXPECT_TRUE(*response == boost::json::parse(R"JSON({ "computed": "world_50"})JSON").as_object());
        });
    }
}

TEST_F(RPCEngineTest, NotCacheIfErrorHappen)
{
    auto const cfgCache = ClioConfigDefinition{
        {"server.max_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(2)},
        {"workers", ConfigValue{ConfigType::Integer}.defaultValue(4).withConstraint(validateUint16)},
        {"rpc.cache_timeout", ConfigValue{ConfigType::Double}.defaultValue(10.0).withConstraint(validatePositiveDouble)}
    };

    auto const notAdmin = false;
    auto const method = "server_info";
    std::shared_ptr<RPCEngine<MockLoadBalancer, MockCounters>> engine =
        RPCEngine<MockLoadBalancer, MockCounters>::make_RPCEngine(
            cfgCache, backend, mockLoadBalancerPtr, dosGuard, queue, *mockCountersPtr, handlerProvider
        );

    int callTime = 2;
    EXPECT_CALL(*backend, isTooBusy).Times(callTime).WillRepeatedly(Return(false));
    EXPECT_CALL(*handlerProvider, getHandler)
        .Times(callTime)
        .WillRepeatedly(Return(AnyHandler{tests::common::FailingHandlerFake{}}));
    EXPECT_CALL(*mockCountersPtr, rpcErrored(method)).Times(callTime);
    EXPECT_CALL(*handlerProvider, isClioOnly).Times(callTime).WillRepeatedly(Return(false));
    EXPECT_CALL(*handlerProvider, contains).Times(callTime).WillRepeatedly(Return(true));

    while (callTime-- != 0) {
        runSpawn([&](auto yield) {
            auto const ctx = web::Context(
                yield,
                method,
                1,
                boost::json::parse(R"JSON({"hello": "world","limit": 50})JSON").as_object(),
                nullptr,
                tagFactory,
                LedgerRange{0, 30},
                "127.0.0.2",
                notAdmin
            );

            auto const res = engine->buildResponse(ctx);
            auto const error = std::get_if<rpc::Status>(&res.response);
            EXPECT_TRUE(*error == rpc::Status{"Very custom error"});
        });
    }
}
