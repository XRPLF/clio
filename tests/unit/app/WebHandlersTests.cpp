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

#include "app/WebHandlers.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/Taggable.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/AdminVerificationStrategy.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/dosguard/DOSGuardMock.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/MockConnection.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using namespace app;
namespace http = boost::beast::http;
using namespace util::config;

struct WebHandlersTest : virtual NoLoggerFixture {
    DOSGuardStrictMock dosGuardMock;
    util::TagDecoratorFactory const tagFactory{
        ClioConfigDefinition{{"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")}}
    };
    std::string const ip = "some ip";
    StrictMockConnection connectionMock{ip, boost::beast::flat_buffer{}, tagFactory};

    struct AdminVerificationStrategyMock : web::AdminVerificationStrategy {
        MOCK_METHOD(bool, isAdmin, (RequestHeader const&, std::string_view), (const, override));
    };
    using AdminVerificationStrategyStrictMockPtr = std::shared_ptr<testing::StrictMock<AdminVerificationStrategyMock>>;
};

struct OnConnectCheckTests : WebHandlersTest {
    OnConnectCheck onConnectCheck{dosGuardMock};
};

TEST_F(OnConnectCheckTests, Ok)
{
    EXPECT_CALL(dosGuardMock, increment(ip));
    EXPECT_CALL(dosGuardMock, isOk(ip)).WillOnce(testing::Return(true));
    EXPECT_TRUE(onConnectCheck(connectionMock).has_value());
}

TEST_F(OnConnectCheckTests, RateLimited)
{
    EXPECT_CALL(dosGuardMock, increment(ip));
    EXPECT_CALL(dosGuardMock, isOk(ip)).WillOnce(testing::Return(false));
    EXPECT_CALL(connectionMock, wasUpgraded).WillOnce(testing::Return(false));

    auto response = onConnectCheck(connectionMock);
    ASSERT_FALSE(response.has_value());
    auto const httpResponse = std::move(response).error().intoHttpResponse();
    EXPECT_EQ(httpResponse.result(), boost::beast::http::status::too_many_requests);
    EXPECT_EQ(httpResponse.body(), "Too many requests");
}

struct DisconnectHookTests : WebHandlersTest {
    DisconnectHook disconnectHook{dosGuardMock};
};

TEST_F(DisconnectHookTests, CallsDecrement)
{
    EXPECT_CALL(dosGuardMock, decrement(ip));
    disconnectHook(connectionMock);
}

struct MetricsHandlerTests : util::prometheus::WithPrometheus, SyncAsioContextTest, WebHandlersTest {
    AdminVerificationStrategyStrictMockPtr adminVerifier{
        std::make_shared<testing::StrictMock<AdminVerificationStrategyMock>>()
    };

    MetricsHandler metricsHandler{adminVerifier};
    web::ng::Request request{http::request<http::string_body>{http::verb::get, "/metrics", 11}};
};

TEST_F(MetricsHandlerTests, Call)
{
    EXPECT_CALL(*adminVerifier, isAdmin).WillOnce(testing::Return(true));
    runSpawn([&](boost::asio::yield_context yield) {
        auto response = metricsHandler(request, connectionMock, nullptr, yield);
        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), boost::beast::http::status::ok);
    });
}

struct HealthCheckHandlerTests : SyncAsioContextTest, WebHandlersTest {
    web::ng::Request request{http::request<http::string_body>{http::verb::get, "/", 11}};
    HealthCheckHandler healthCheckHandler;
};

TEST_F(HealthCheckHandlerTests, Call)
{
    runSpawn([&](boost::asio::yield_context yield) {
        auto response = healthCheckHandler(request, connectionMock, nullptr, yield);
        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), boost::beast::http::status::ok);
    });
}

struct RequestHandlerTest : SyncAsioContextTest, WebHandlersTest {
    AdminVerificationStrategyStrictMockPtr adminVerifier{
        std::make_shared<testing::StrictMock<AdminVerificationStrategyMock>>()
    };

    struct RpcHandlerMock {
        MOCK_METHOD(
            web::ng::Response,
            call,
            (web::ng::Request const&,
             web::ng::ConnectionMetadata const&,
             web::SubscriptionContextPtr,
             boost::asio::yield_context),
            ()
        );

        web::ng::Response
        operator()(
            web::ng::Request const& request,
            web::ng::ConnectionMetadata const& connectionMetadata,
            web::SubscriptionContextPtr subscriptionContext,
            boost::asio::yield_context yield
        )
        {
            return call(request, connectionMetadata, std::move(subscriptionContext), yield);
        }
    };

    testing::StrictMock<RpcHandlerMock> rpcHandler;
    StrictMockConnection connectionMock{ip, boost::beast::flat_buffer{}, tagFactory};
    RequestHandler<RpcHandlerMock> requestHandler{adminVerifier, rpcHandler, dosGuardMock};
};

TEST_F(RequestHandlerTest, DosguardRateLimited_Http)
{
    web::ng::Request const request{http::request<http::string_body>{http::verb::get, "/", 11}};

    EXPECT_CALL(dosGuardMock, request(ip)).WillOnce(testing::Return(false));

    runSpawn([&](boost::asio::yield_context yield) {
        auto response = requestHandler(request, connectionMock, nullptr, yield);
        auto const httpResponse = std::move(response).intoHttpResponse();

        EXPECT_EQ(httpResponse.result(), boost::beast::http::status::service_unavailable);

        auto const body = boost::json::parse(httpResponse.body()).as_object();
        EXPECT_EQ(body.at("error").as_string(), "slowDown");
        EXPECT_EQ(body.at("error_code").as_int64(), 10);
        EXPECT_EQ(body.at("status").as_string(), "error");
        EXPECT_FALSE(body.contains("id"));
        EXPECT_FALSE(body.contains("request"));
    });
}

TEST_F(RequestHandlerTest, DosguardRateLimited_Ws)
{
    auto const requestMessage = R"json({"some": "request", "id": "some id"})json";
    web::ng::Request::HttpHeaders const headers{};
    web::ng::Request const request{requestMessage, headers};

    EXPECT_CALL(dosGuardMock, request(ip)).WillOnce(testing::Return(false));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const response = requestHandler(request, connectionMock, nullptr, yield);
        auto const message = boost::json::parse(response.message()).as_object();

        EXPECT_EQ(message.at("error").as_string(), "slowDown");
        EXPECT_EQ(message.at("error_code").as_int64(), 10);
        EXPECT_EQ(message.at("status").as_string(), "error");
        EXPECT_EQ(message.at("id").as_string(), "some id");
        EXPECT_EQ(message.at("request").as_string(), requestMessage);
    });
}

TEST_F(RequestHandlerTest, DosguardRateLimited_Ws_ErrorParsing)
{
    auto const requestMessage = R"json(some request "id": "some id")json";
    web::ng::Request::HttpHeaders const headers{};
    web::ng::Request const request{requestMessage, headers};

    EXPECT_CALL(dosGuardMock, request(ip)).WillOnce(testing::Return(false));

    runSpawn([&](boost::asio::yield_context yield) {
        auto const response = requestHandler(request, connectionMock, nullptr, yield);
        auto const message = boost::json::parse(response.message()).as_object();

        EXPECT_EQ(message.at("error").as_string(), "slowDown");
        EXPECT_EQ(message.at("error_code").as_int64(), 10);
        EXPECT_EQ(message.at("status").as_string(), "error");
        EXPECT_FALSE(message.contains("id"));
        EXPECT_EQ(message.at("request").as_string(), requestMessage);
    });
}

TEST_F(RequestHandlerTest, RpcHandlerThrows)
{
    web::ng::Request const request{http::request<http::string_body>{http::verb::get, "/", 11}};

    EXPECT_CALL(dosGuardMock, request(ip)).WillOnce(testing::Return(true));
    EXPECT_CALL(*adminVerifier, isAdmin).WillOnce(testing::Return(true));
    EXPECT_CALL(rpcHandler, call).WillOnce(testing::Throw(std::runtime_error{"some error"}));

    runSpawn([&](boost::asio::yield_context yield) {
        auto response = requestHandler(request, connectionMock, nullptr, yield);

        auto const httpResponse = std::move(response).intoHttpResponse();

        EXPECT_EQ(httpResponse.result(), boost::beast::http::status::internal_server_error);

        auto const body = boost::json::parse(httpResponse.body()).as_object();
        EXPECT_EQ(body.at("error").as_string(), "internal");
        EXPECT_EQ(body.at("error_code").as_int64(), 73);
        EXPECT_EQ(body.at("status").as_string(), "error");
    });
}

TEST_F(RequestHandlerTest, NoErrors)
{
    web::ng::Request const request{http::request<http::string_body>{http::verb::get, "/", 11}};
    web::ng::Response const response{http::status::ok, "some response", request};
    auto const httpResponse = web::ng::Response{response}.intoHttpResponse();

    EXPECT_CALL(dosGuardMock, request(ip)).WillOnce(testing::Return(true));
    EXPECT_CALL(*adminVerifier, isAdmin).WillOnce(testing::Return(true));
    EXPECT_CALL(rpcHandler, call).WillOnce(testing::Return(response));
    EXPECT_CALL(dosGuardMock, add(ip, testing::_)).WillOnce(testing::Return(true));

    runSpawn([&](boost::asio::yield_context yield) {
        auto actualResponse = requestHandler(request, connectionMock, nullptr, yield);

        auto const actualHttpResponse = std::move(actualResponse).intoHttpResponse();

        EXPECT_EQ(actualHttpResponse.result(), httpResponse.result());
        EXPECT_EQ(actualHttpResponse.body(), httpResponse.body());
        EXPECT_EQ(actualHttpResponse.version(), 11);
    });
}

TEST_F(RequestHandlerTest, ResponseDosGuardWarning_ResponseHasWarnings)
{
    web::ng::Request const request{http::request<http::string_body>{http::verb::get, "/", 11}};
    web::ng::Response const response{
        http::status::ok, R"json({"some":"response", "warnings":["some warning"]})json", request
    };
    auto const httpResponse = web::ng::Response{response}.intoHttpResponse();

    EXPECT_CALL(dosGuardMock, request(ip)).WillOnce(testing::Return(true));
    EXPECT_CALL(*adminVerifier, isAdmin).WillOnce(testing::Return(true));
    EXPECT_CALL(rpcHandler, call).WillOnce(testing::Return(response));
    EXPECT_CALL(dosGuardMock, add(ip, testing::_)).WillOnce(testing::Return(false));

    runSpawn([&](boost::asio::yield_context yield) {
        auto actualResponse = requestHandler(request, connectionMock, nullptr, yield);

        auto const actualHttpResponse = std::move(actualResponse).intoHttpResponse();

        EXPECT_EQ(actualHttpResponse.result(), httpResponse.result());
        EXPECT_EQ(actualHttpResponse.version(), 11);

        auto actualBody = boost::json::parse(actualHttpResponse.body()).as_object();
        EXPECT_EQ(actualBody.at("some").as_string(), "response");
        EXPECT_EQ(actualBody.at("warnings").as_array().size(), 2);
    });
}

TEST_F(RequestHandlerTest, ResponseDosGuardWarning_ResponseDoesntHaveWarnings)
{
    web::ng::Request const request{http::request<http::string_body>{http::verb::get, "/", 11}};
    web::ng::Response const response{http::status::ok, R"json({"some":"response"})json", request};
    auto const httpResponse = web::ng::Response{response}.intoHttpResponse();

    EXPECT_CALL(dosGuardMock, request(ip)).WillOnce(testing::Return(true));
    EXPECT_CALL(*adminVerifier, isAdmin).WillOnce(testing::Return(true));
    EXPECT_CALL(rpcHandler, call).WillOnce(testing::Return(response));
    EXPECT_CALL(dosGuardMock, add(ip, testing::_)).WillOnce(testing::Return(false));

    runSpawn([&](boost::asio::yield_context yield) {
        auto actualResponse = requestHandler(request, connectionMock, nullptr, yield);

        auto const actualHttpResponse = std::move(actualResponse).intoHttpResponse();

        EXPECT_EQ(actualHttpResponse.result(), httpResponse.result());
        EXPECT_EQ(actualHttpResponse.version(), 11);

        auto actualBody = boost::json::parse(actualHttpResponse.body()).as_object();
        EXPECT_EQ(actualBody.at("some").as_string(), "response");
        EXPECT_EQ(actualBody.at("warnings").as_array().size(), 1);
    });
}
