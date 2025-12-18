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
#include "rpc/Errors.hpp"
#include "rpc/WorkQueue.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockLedgerCache.hpp"
#include "util/MockPrometheus.hpp"
#include "util/Taggable.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
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

struct WebHandlersTest : virtual public ::testing::Test {
    DOSGuardStrictMock dosGuardMock;
    util::TagDecoratorFactory const tagFactory{
        ClioConfigDefinition{{"log.tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")}}
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

struct IpChangeHookTests : WebHandlersTest {
    IpChangeHook ipChangeHook{dosGuardMock};
};

TEST_F(IpChangeHookTests, CallsDecrementAndIncrement)
{
    std::string const oldIp = "old ip";
    std::string const newIp = "new ip";

    EXPECT_CALL(dosGuardMock, decrement(oldIp));
    EXPECT_CALL(dosGuardMock, increment(newIp));

    ipChangeHook(oldIp, newIp);
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

    rpc::WorkQueue workQueue{1};

    MetricsHandler metricsHandler{adminVerifier, workQueue};
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

struct CacheStateHandlerTests : SyncAsioContextTest, WebHandlersTest {
    web::ng::Request request{http::request<http::string_body>{http::verb::get, "/", 11}};
    MockLedgerCache cache;
    CacheStateHandler cacheStateHandler{cache};
};

TEST_F(CacheStateHandlerTests, CallWithCacheLoaded)
{
    EXPECT_CALL(cache, isFull()).WillRepeatedly(testing::Return(true));
    runSpawn([&](boost::asio::yield_context yield) {
        auto response = cacheStateHandler(request, connectionMock, nullptr, yield);
        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), boost::beast::http::status::ok);
    });
}

TEST_F(CacheStateHandlerTests, CallWithoutCacheLoaded)
{
    EXPECT_CALL(cache, isFull()).WillRepeatedly(testing::Return(false));
    runSpawn([&](boost::asio::yield_context yield) {
        auto response = cacheStateHandler(request, connectionMock, nullptr, yield);
        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), boost::beast::http::status::service_unavailable);
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
    RequestHandler<RpcHandlerMock> requestHandler{adminVerifier, rpcHandler};
};

TEST_F(RequestHandlerTest, RpcHandlerThrows)
{
    web::ng::Request const request{http::request<http::string_body>{http::verb::get, "/", 11}};

    EXPECT_CALL(*adminVerifier, isAdmin).WillOnce(testing::Return(true));
    EXPECT_CALL(rpcHandler, call).WillOnce(testing::Throw(std::runtime_error{"some error"}));

    runSpawn([&](boost::asio::yield_context yield) {
        auto response = requestHandler(request, connectionMock, nullptr, yield);

        auto const httpResponse = std::move(response).intoHttpResponse();

        EXPECT_EQ(httpResponse.result(), boost::beast::http::status::internal_server_error);

        auto const body = boost::json::parse(httpResponse.body()).as_object();
        EXPECT_EQ(body.at("error").as_string(), "internal");
        EXPECT_EQ(body.at("error_code").as_int64(), rpc::RippledError::rpcINTERNAL);
        EXPECT_EQ(body.at("status").as_string(), "error");
    });
}

TEST_F(RequestHandlerTest, NoErrors)
{
    web::ng::Request const request{http::request<http::string_body>{http::verb::get, "/", 11}};
    web::ng::Response const response{http::status::ok, "some response", request};
    auto const httpResponse = web::ng::Response{response}.intoHttpResponse();

    EXPECT_CALL(*adminVerifier, isAdmin).WillOnce(testing::Return(true));
    EXPECT_CALL(rpcHandler, call).WillOnce(testing::Return(response));

    runSpawn([&](boost::asio::yield_context yield) {
        auto actualResponse = requestHandler(request, connectionMock, nullptr, yield);

        auto const actualHttpResponse = std::move(actualResponse).intoHttpResponse();

        EXPECT_EQ(actualHttpResponse.result(), httpResponse.result());
        EXPECT_EQ(actualHttpResponse.body(), httpResponse.body());
        EXPECT_EQ(actualHttpResponse.version(), 11);
    });
}
