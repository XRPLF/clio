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

#include "rpc/Errors.hpp"
#include "rpc/common/Types.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockETLService.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockRPCEngine.hpp"
#include "util/Taggable.hpp"
#include "util/config/Config.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/ng/MockConnection.hpp"
#include "web/ng/RPCServerHandler.hpp"
#include "web/ng/Request.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

using namespace web::ng;
using testing::Return;
using testing::StrictMock;
using namespace util::config;

namespace http = boost::beast::http;

struct ng_RPCServerHandlerTest : util::prometheus::WithPrometheus, MockBackendTestStrict, SyncAsioContextTest {
    ClioConfigDefinition config{ClioConfigDefinition{
        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")},
        {"api_version.min", ConfigValue{ConfigType::Integer}.defaultValue(1)},
        {"api_version.max", ConfigValue{ConfigType::Integer}.defaultValue(2)},
        {"api_version.default", ConfigValue{ConfigType::Integer}.defaultValue(1)}
    }};
    std::shared_ptr<testing::StrictMock<MockRPCEngine>> rpcEngine_ =
        std::make_shared<testing::StrictMock<MockRPCEngine>>();
    std::shared_ptr<StrictMock<MockETLService>> etl_ = std::make_shared<StrictMock<MockETLService>>();
    RPCServerHandler<MockRPCEngine, MockETLService> rpcServerHandler_{config, backend, rpcEngine_, etl_};

    util::TagDecoratorFactory tagFactory_{config};
    StrictMockConnectionMetadata connectionMetadata_{"some ip", tagFactory_};

    static Request
    makeHttpRequest(std::string_view body)
    {
        return Request{http::request<http::string_body>{http::verb::post, "/", 11, body}};
    }
};

TEST_F(ng_RPCServerHandlerTest, PostToRpcEngineFailed)
{
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest("some message");

        EXPECT_CALL(*rpcEngine_, post).WillOnce(Return(false));
        EXPECT_CALL(*rpcEngine_, notifyTooBusy());
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);

        EXPECT_EQ(std::move(response).intoHttpResponse().result(), http::status::service_unavailable);
    });
}

TEST_F(ng_RPCServerHandlerTest, CoroutineSleepsUntilRpcEngineFinishes)
{
    StrictMock<testing::MockFunction<void()>> rpcServerHandlerDone;
    StrictMock<testing::MockFunction<void()>> rpcEngineDone;
    testing::Expectation const expectedRpcEngineDone = EXPECT_CALL(rpcEngineDone, Call);
    EXPECT_CALL(rpcServerHandlerDone, Call).After(expectedRpcEngineDone);

    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest("some message");

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            boost::asio::spawn(
                ctx,
                [this, &rpcEngineDone, fn = std::forward<decltype(fn)>(fn)](boost::asio::yield_context yield) {
                    EXPECT_CALL(*rpcEngine_, notifyBadSyntax);
                    fn(yield);
                    rpcEngineDone.Call();
                }
            );
            return true;
        });

        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);
        rpcServerHandlerDone.Call();

        EXPECT_EQ(std::move(response).intoHttpResponse().result(), http::status::bad_request);
    });
}

TEST_F(ng_RPCServerHandlerTest, JsonParseFailed)
{
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest("not a json");

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(*rpcEngine_, notifyBadSyntax);
            fn(yield);
            return true;
        });
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);
        EXPECT_EQ(std::move(response).intoHttpResponse().result(), http::status::bad_request);
    });
}

TEST_F(ng_RPCServerHandlerTest, GotNotJsonObject)
{
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest("[]");
        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(*rpcEngine_, notifyBadSyntax);
            fn(yield);
            return true;
        });
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);
        EXPECT_EQ(std::move(response).intoHttpResponse().result(), http::status::bad_request);
    });
}

TEST_F(ng_RPCServerHandlerTest, HandleRequest_NoRangeFromBackend)
{
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest("{}");

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(connectionMetadata_, wasUpgraded).WillOnce(Return(not request.isHttp()));
            EXPECT_CALL(*rpcEngine_, notifyNotReady);
            fn(yield);
            return true;
        });
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);

        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), http::status::ok);

        auto const jsonResponse = boost::json::parse(httpResponse.body()).as_object();
        EXPECT_EQ(jsonResponse.at("result").at("error").as_string(), "notReady");
    });
}

TEST_F(ng_RPCServerHandlerTest, HandleRequest_ContextCreationFailed)
{
    backend->setRange(0, 1);
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest("{}");

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(connectionMetadata_, wasUpgraded).WillRepeatedly(Return(not request.isHttp()));
            EXPECT_CALL(*rpcEngine_, notifyBadSyntax);
            fn(yield);
            return true;
        });
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);

        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), http::status::bad_request);
        EXPECT_EQ(httpResponse.body(), "Null method");
    });
}

TEST_F(ng_RPCServerHandlerTest, HandleRequest_BuildResponseFailed)
{
    backend->setRange(0, 1);
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest(R"json({"method":"some_method"})json");

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(connectionMetadata_, wasUpgraded).WillRepeatedly(Return(not request.isHttp()));
            EXPECT_CALL(*rpcEngine_, buildResponse)
                .WillOnce(Return(rpc::Result{rpc::Status{rpc::ClioError::rpcUNKNOWN_OPTION}}));
            EXPECT_CALL(*etl_, lastCloseAgeSeconds).WillOnce(Return(1));
            fn(yield);
            return true;
        });
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);

        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), http::status::ok);

        auto const jsonResponse = boost::json::parse(httpResponse.body()).as_object();
        EXPECT_EQ(jsonResponse.at("result").at("error").as_string(), "unknownOption");

        ASSERT_EQ(jsonResponse.at("warnings").as_array().size(), 1);
        EXPECT_EQ(jsonResponse.at("warnings").as_array().at(0).as_object().at("id").as_int64(), rpc::warnRPC_CLIO);
    });
}

TEST_F(ng_RPCServerHandlerTest, HandleRequest_BuildResponseThrewAnException)
{
    backend->setRange(0, 1);
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest(R"json({"method":"some_method"})json");

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(connectionMetadata_, wasUpgraded).WillRepeatedly(Return(not request.isHttp()));
            EXPECT_CALL(*rpcEngine_, buildResponse).WillOnce([](auto&&) -> rpc::Result {
                throw std::runtime_error("some error");
            });
            EXPECT_CALL(*rpcEngine_, notifyInternalError);
            fn(yield);
            return true;
        });
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);

        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), http::status::internal_server_error);
    });
}

TEST_F(ng_RPCServerHandlerTest, HandleRequest_Successful_HttpRequest)
{
    backend->setRange(0, 1);
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest(R"json({"method":"some_method"})json");

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(connectionMetadata_, wasUpgraded).WillRepeatedly(Return(not request.isHttp()));
            EXPECT_CALL(*rpcEngine_, buildResponse)
                .WillOnce(Return(rpc::Result{rpc::ReturnType{boost::json::object{{"some key", "some value"}}}}));
            EXPECT_CALL(*rpcEngine_, notifyComplete);
            EXPECT_CALL(*etl_, lastCloseAgeSeconds).WillOnce(Return(1));
            fn(yield);
            return true;
        });
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);

        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), http::status::ok);

        auto const jsonResponse = boost::json::parse(httpResponse.body()).as_object();
        EXPECT_EQ(jsonResponse.at("result").at("some key").as_string(), "some value");
        EXPECT_EQ(jsonResponse.at("result").at("status").as_string(), "success");

        ASSERT_EQ(jsonResponse.at("warnings").as_array().size(), 1) << jsonResponse;
        EXPECT_EQ(jsonResponse.at("warnings").as_array().at(0).as_object().at("id").as_int64(), rpc::warnRPC_CLIO);
    });
}

TEST_F(ng_RPCServerHandlerTest, HandleRequest_OutdatedWarning)
{
    backend->setRange(0, 1);
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest(R"json({"method":"some_method"})json");

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(connectionMetadata_, wasUpgraded).WillRepeatedly(Return(not request.isHttp()));
            EXPECT_CALL(*rpcEngine_, buildResponse)
                .WillOnce(Return(rpc::Result{rpc::ReturnType{boost::json::object{{"some key", "some value"}}}}));
            EXPECT_CALL(*rpcEngine_, notifyComplete);
            EXPECT_CALL(*etl_, lastCloseAgeSeconds).WillOnce(Return(61));
            fn(yield);
            return true;
        });
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);

        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), http::status::ok);

        auto const jsonResponse = boost::json::parse(httpResponse.body()).as_object();

        std::unordered_set<int64_t> warningCodes;
        std::ranges::transform(
            jsonResponse.at("warnings").as_array(),
            std::inserter(warningCodes, warningCodes.end()),
            [](auto const& w) { return w.as_object().at("id").as_int64(); }
        );

        EXPECT_EQ(warningCodes.size(), 2);
        EXPECT_TRUE(warningCodes.contains(rpc::warnRPC_CLIO));
        EXPECT_TRUE(warningCodes.contains(rpc::warnRPC_OUTDATED));
    });
}

TEST_F(ng_RPCServerHandlerTest, HandleRequest_Successful_HttpRequest_Forwarded)
{
    backend->setRange(0, 1);
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest(R"json({"method":"some_method"})json");

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(connectionMetadata_, wasUpgraded).WillRepeatedly(Return(not request.isHttp()));
            EXPECT_CALL(*rpcEngine_, buildResponse)
                .WillOnce(Return(rpc::Result{rpc::ReturnType{boost::json::object{
                    {"result", boost::json::object{{"some key", "some value"}}}, {"forwarded", true}
                }}}));
            EXPECT_CALL(*rpcEngine_, notifyComplete);
            EXPECT_CALL(*etl_, lastCloseAgeSeconds).WillOnce(Return(1));
            fn(yield);
            return true;
        });
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);

        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), http::status::ok);

        auto const jsonResponse = boost::json::parse(httpResponse.body()).as_object();
        EXPECT_EQ(jsonResponse.at("result").at("some key").as_string(), "some value");
        EXPECT_EQ(jsonResponse.at("result").at("status").as_string(), "success");
        EXPECT_EQ(jsonResponse.at("forwarded").as_bool(), true);

        ASSERT_EQ(jsonResponse.at("warnings").as_array().size(), 1) << jsonResponse;
        EXPECT_EQ(jsonResponse.at("warnings").as_array().at(0).as_object().at("id").as_int64(), rpc::warnRPC_CLIO);
    });
}

TEST_F(ng_RPCServerHandlerTest, HandleRequest_Successful_HttpRequest_HasError)
{
    backend->setRange(0, 1);
    runSpawn([&](boost::asio::yield_context yield) {
        auto const request = makeHttpRequest(R"json({"method":"some_method"})json");

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(connectionMetadata_, wasUpgraded).WillRepeatedly(Return(not request.isHttp()));
            EXPECT_CALL(*rpcEngine_, buildResponse)
                .WillOnce(Return(rpc::Result{
                    rpc::ReturnType{boost::json::object{{"some key", "some value"}, {"error", "some error"}}}
                }));
            EXPECT_CALL(*rpcEngine_, notifyComplete);
            EXPECT_CALL(*etl_, lastCloseAgeSeconds).WillOnce(Return(1));
            fn(yield);
            return true;
        });
        auto response = rpcServerHandler_(request, connectionMetadata_, nullptr, yield);

        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), http::status::ok);

        auto const jsonResponse = boost::json::parse(httpResponse.body()).as_object();
        EXPECT_EQ(jsonResponse.at("result").at("some key").as_string(), "some value");
        EXPECT_EQ(jsonResponse.at("result").at("error").as_string(), "some error");

        ASSERT_EQ(jsonResponse.at("warnings").as_array().size(), 1) << jsonResponse;
        EXPECT_EQ(jsonResponse.at("warnings").as_array().at(0).as_object().at("id").as_int64(), rpc::warnRPC_CLIO);
    });
}

struct ng_RPCServerHandlerWsTest : ng_RPCServerHandlerTest {
    struct MockSubscriptionContext : web::SubscriptionContextInterface {
        using web::SubscriptionContextInterface::SubscriptionContextInterface;

        MOCK_METHOD(void, send, (std::shared_ptr<std::string>), (override));
        MOCK_METHOD(void, onDisconnect, (web::SubscriptionContextInterface::OnDisconnectSlot const&), (override));
        MOCK_METHOD(void, setApiSubversion, (uint32_t), (override));
        MOCK_METHOD(uint32_t, apiSubversion, (), (const, override));
    };
    using StrictMockSubscriptionContext = testing::StrictMock<MockSubscriptionContext>;

    std::shared_ptr<StrictMockSubscriptionContext> subscriptionContext_ =
        std::make_shared<StrictMockSubscriptionContext>(tagFactory_);
};

TEST_F(ng_RPCServerHandlerWsTest, HandleRequest_Successful_WsRequest)
{
    backend->setRange(0, 1);
    runSpawn([&](boost::asio::yield_context yield) {
        Request::HttpHeaders const headers;
        auto const request = Request(R"json({"method":"some_method", "id": 1234, "api_version": 1})json", headers);

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(connectionMetadata_, wasUpgraded).WillRepeatedly(Return(not request.isHttp()));
            EXPECT_CALL(*rpcEngine_, buildResponse)
                .WillOnce(Return(rpc::Result{rpc::ReturnType{boost::json::object{{"some key", "some value"}}}}));
            EXPECT_CALL(*rpcEngine_, notifyComplete);
            EXPECT_CALL(*etl_, lastCloseAgeSeconds).WillOnce(Return(1));
            fn(yield);
            return true;
        });
        auto const response = rpcServerHandler_(request, connectionMetadata_, subscriptionContext_, yield);

        auto const jsonResponse = boost::json::parse(response.message()).as_object();
        EXPECT_EQ(jsonResponse.at("result").at("some key").as_string(), "some value");
        EXPECT_EQ(jsonResponse.at("status").as_string(), "success");

        EXPECT_EQ(jsonResponse.at("type").as_string(), "response");
        EXPECT_EQ(jsonResponse.at("id").as_int64(), 1234);
        EXPECT_EQ(jsonResponse.at("api_version").as_int64(), 1);

        ASSERT_EQ(jsonResponse.at("warnings").as_array().size(), 1) << jsonResponse;
        EXPECT_EQ(jsonResponse.at("warnings").as_array().at(0).as_object().at("id").as_int64(), rpc::warnRPC_CLIO);
    });
}

TEST_F(ng_RPCServerHandlerWsTest, HandleRequest_Successful_WsRequest_HasError)
{
    backend->setRange(0, 1);
    runSpawn([&](boost::asio::yield_context yield) {
        Request::HttpHeaders const headers;
        auto const request = Request(R"json({"method":"some_method", "id": 1234, "api_version": 1})json", headers);

        EXPECT_CALL(*rpcEngine_, post).WillOnce([&](auto&& fn, auto&&) {
            EXPECT_CALL(connectionMetadata_, wasUpgraded).WillRepeatedly(Return(not request.isHttp()));
            EXPECT_CALL(*rpcEngine_, buildResponse)
                .WillOnce(Return(rpc::Result{
                    rpc::ReturnType{boost::json::object{{"some key", "some value"}, {"error", "some error"}}}
                }));
            EXPECT_CALL(*rpcEngine_, notifyComplete);
            EXPECT_CALL(*etl_, lastCloseAgeSeconds).WillOnce(Return(1));
            fn(yield);
            return true;
        });
        auto const response = rpcServerHandler_(request, connectionMetadata_, subscriptionContext_, yield);

        auto const jsonResponse = boost::json::parse(response.message()).as_object();
        EXPECT_EQ(jsonResponse.at("result").at("some key").as_string(), "some value");
        EXPECT_EQ(jsonResponse.at("result").at("error").as_string(), "some error");

        EXPECT_EQ(jsonResponse.at("type").as_string(), "response");
        EXPECT_EQ(jsonResponse.at("id").as_int64(), 1234);
        EXPECT_EQ(jsonResponse.at("api_version").as_int64(), 1);

        ASSERT_EQ(jsonResponse.at("warnings").as_array().size(), 1) << jsonResponse;
        EXPECT_EQ(jsonResponse.at("warnings").as_array().at(0).as_object().at("id").as_int64(), rpc::warnRPC_CLIO);
    });
}
