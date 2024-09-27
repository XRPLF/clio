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
#include "data/Types.hpp"
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
#include "util/config/Config.hpp"
#include "web/Context.hpp"
#include "web/dosguard/DOSGuard.hpp"
#include "web/dosguard/WhitelistHandler.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/ErrorCodes.h>

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

namespace {
constexpr auto FORWARD_REPLY = R"JSON({
    "result": 
    {
        "status": "success",
        "forwarded": true
    }
})JSON";
}  // namespace

class RPCEngineTest : public util::prometheus::WithPrometheus,
                      public MockBackendTest,
                      public MockCountersTest,
                      public MockLoadBalancerTest,
                      public SyncAsioContextTest {
protected:
    Config cfg = Config{json::parse(R"JSON({
        "server": {"max_queue_size": 2},
        "workers": 4
    })JSON")};
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
    auto const isAdmin = true;
    auto const isTooBusy = true;
    auto const neverCalledIsTooBusy = std::nullopt;
    auto const neverCalledUnknownCmd = std::nullopt;
    auto const forwarded = true;
    auto const cmdUnknown = true;
    auto const handlerReturnError = true;

    return std::vector<RPCEngineFlowTestCaseBundle>{
        {"ForwardedSubmit",
         isAdmin,
         "submit",
         "{}",
         forwarded,
         neverCalledIsTooBusy,
         neverCalledUnknownCmd,
         !handlerReturnError,
         rpc::Status{},
         boost::json::parse(FORWARD_REPLY).as_object()},
        {"ForwardAdminCmd",
         !isAdmin,
         "ledger",
         R"JSON({"full": true, "ledger_index": "current"})JSON",
         !forwarded,
         neverCalledIsTooBusy,
         neverCalledUnknownCmd,
         !handlerReturnError,
         rpc::Status{RippledError::rpcNO_PERMISSION},
         std::nullopt},
        {"BackendTooBusy",
         !isAdmin,
         "ledger",
         "{}",
         !forwarded,
         isTooBusy,
         neverCalledUnknownCmd,
         !handlerReturnError,
         rpc::Status{RippledError::rpcTOO_BUSY},
         std::nullopt},
        {"HandlerUnknown",
         !isAdmin,
         "ledger",
         "{}",
         !forwarded,
         neverCalledIsTooBusy,
         cmdUnknown,
         !handlerReturnError,
         rpc::Status{RippledError::rpcUNKNOWN_COMMAND},
         std::nullopt},
        {"HandlerReturnError",
         !isAdmin,
         "ledger",
         R"JSON({"hello": "world", "limit": 50})JSON",
         !forwarded,
         neverCalledIsTooBusy,
         !cmdUnknown,
         handlerReturnError,
         rpc::Status{"Very custom error"},
         std::nullopt},
        {"HandlerReturnResponse",
         !isAdmin,
         "ledger",
         R"JSON({"hello": "world", "limit": 50})JSON",
         !forwarded,
         neverCalledIsTooBusy,
         !cmdUnknown,
         !handlerReturnError,
         std::nullopt,
         boost::json::parse(R"JSON({"computed": "world_50"})JSON").as_object()},
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
    auto const testBundle = GetParam();

    std::shared_ptr<RPCEngine<MockLoadBalancer, MockCounters>> engine =
        RPCEngine<MockLoadBalancer, MockCounters>::make_RPCEngine(
            Config{}, backend, mockLoadBalancerPtr, dosGuard, queue, *mockCountersPtr, handlerProvider
        );

    if (testBundle.forwarded) {
        EXPECT_CALL(*mockLoadBalancerPtr, forwardToRippled)
            .WillOnce(Return(std::expected<boost::json::object, rpc::ClioError>(json::parse(FORWARD_REPLY).as_object()))
            );
        EXPECT_CALL(*handlerProvider, contains).WillOnce(Return(true));
        EXPECT_CALL(*mockCountersPtr, rpcForwarded(testBundle.method)).Times(1);
    }

    if (testBundle.isTooBusy.has_value()) {
        EXPECT_CALL(*backend, isTooBusy).WillOnce(Return(*testBundle.isTooBusy));
        EXPECT_CALL(*mockCountersPtr, onTooBusy).Times(1);
    }

    EXPECT_CALL(*handlerProvider, isClioOnly).WillOnce(Return(false));

    if (testBundle.isUnknownCmd.has_value()) {
        if (testBundle.isUnknownCmd.value()) {
            EXPECT_CALL(*handlerProvider, getHandler).WillOnce(Return(std::nullopt));
            EXPECT_CALL(*mockCountersPtr, onUnknownCommand).Times(1);
        } else {
            if (testBundle.handlerReturnError) {
                EXPECT_CALL(*handlerProvider, getHandler)
                    .WillOnce(Return(AnyHandler{tests::common::FailingHandlerFake{}}));
                EXPECT_CALL(*mockCountersPtr, rpcErrored(testBundle.method)).Times(1);
                EXPECT_CALL(*handlerProvider, contains(testBundle.method)).WillOnce(Return(true)).Times(1);
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
    EXPECT_CALL(*mockCountersPtr, onTooBusy()).Times(1);

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
    bool isAdmin;
    bool expectedCacheEnabled;
};

struct RPCEngineCacheParameterTest : public RPCEngineTest, WithParamInterface<RPCEngineCacheTestCaseBundle> {};

static auto
generateCacheTestValuesForParametersTest()
{
    auto const isAdmin = true;
    auto const expectedCacheEnabled = true;
    return std::vector<RPCEngineCacheTestCaseBundle>{
        {"CacheEnabled",
         R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": 
            {
                "cache_timeout": 10,
                "cached_commands": ["cache_it"]
            }
         })JSON",
         !isAdmin,
         expectedCacheEnabled},
        {"CacheDisabledWhenNoConfig",
         R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": {}
         })JSON",
         !isAdmin,
         !expectedCacheEnabled},
        {"CacheDisabledWhenNoCmds",
         R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": 
            {
                "cache_timeout": 10,
                "cached_commands": []
            }
         })JSON",
         !isAdmin,
         !expectedCacheEnabled},
        {"CacheDisabledWhenNoTimeout",
         R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": 
            {
                "cached_commands": ["cache_it"]
            }
         })JSON",
         !isAdmin,
         !expectedCacheEnabled},
        {"CacheDisabledWhenCmdNotMatch",
         R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": 
            {
                "cache_timeout": 10,
                "cached_commands": ["cache_it2"]
            }
         })JSON",
         !isAdmin,
         !expectedCacheEnabled},
        {"CacheDisabledWhenTimeoutIsZero",
         R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": 
            {
                "cache_timeout": 0,
                "cached_commands": ["cache_it"]
            }
         })JSON",
         !isAdmin,
         !expectedCacheEnabled},
        {"CacheNotWorkForAdmin",
         R"JSON({      
            "server": {"max_queue_size": 2},
            "workers": 4,
            "rpc": 
            {
                "cache_timeout": 10,
                "cached_commands": ["cache_it"]
            }
         })JSON",
         isAdmin,
         !expectedCacheEnabled},
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
    auto const testParam = GetParam();
    auto const cfgCache = Config{json::parse(testParam.config)};

    auto const admin = testParam.isAdmin;
    auto const method = "cache_it";
    std::shared_ptr<RPCEngine<MockLoadBalancer, MockCounters>> engine =
        RPCEngine<MockLoadBalancer, MockCounters>::make_RPCEngine(
            cfgCache, backend, mockLoadBalancerPtr, dosGuard, queue, *mockCountersPtr, handlerProvider
        );
    int callTime = 2;
    if (testParam.expectedCacheEnabled) {
        EXPECT_CALL(*backend, isTooBusy).WillOnce(Return(false));
        EXPECT_CALL(*handlerProvider, getHandler).WillOnce(Return(AnyHandler{tests::common::HandlerFake{}}));
        EXPECT_CALL(*handlerProvider, isClioOnly).WillOnce(Return(false));

    } else {
        EXPECT_CALL(*backend, isTooBusy).Times(callTime).WillRepeatedly(Return(false));
        EXPECT_CALL(*handlerProvider, getHandler)
            .Times(callTime)
            .WillRepeatedly(Return(AnyHandler{tests::common::HandlerFake{}}));
        EXPECT_CALL(*handlerProvider, isClioOnly).Times(callTime).WillRepeatedly(Return(false));
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
    auto const cfgCache = Config{json::parse(R"JSON({      
                                                      "server": {"max_queue_size": 2},
                                                      "workers": 4,
                                                      "rpc": 
                                                      {
                                                          "cache_timeout": 10,
                                                          "cached_commands": ["cache_it"]
                                                      }
                                                })JSON")};

    auto const notAdmin = false;
    auto const method = "cache_it";
    std::shared_ptr<RPCEngine<MockLoadBalancer, MockCounters>> engine =
        RPCEngine<MockLoadBalancer, MockCounters>::make_RPCEngine(
            cfgCache, backend, mockLoadBalancerPtr, dosGuard, queue, *mockCountersPtr, handlerProvider
        );

    int callTime = 2;
    EXPECT_CALL(*backend, isTooBusy).Times(callTime).WillRepeatedly(Return(false));
    EXPECT_CALL(*handlerProvider, getHandler)
        .Times(callTime)
        .WillRepeatedly(Return(AnyHandler{tests::common::FailingHandlerFake{}}));
    EXPECT_CALL(*mockCountersPtr, rpcErrored("cache_it")).Times(callTime);
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
