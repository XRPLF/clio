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

#include "util/AsioContextTestFixture.hpp"
#include "util/AssignRandomPort.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/NameGenerator.hpp"
#include "util/Taggable.hpp"
#include "util/TestHttpClient.hpp"
#include "util/TestWebSocketClient.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/ProcessingPolicy.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/Server.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string>

using namespace web::ng;
using namespace util::config;

namespace http = boost::beast::http;

struct MakeServerTestBundle {
    std::string testName;
    std::string configJson;
    bool expectSuccess;
};

struct MakeServerTest : NoLoggerFixture, testing::WithParamInterface<MakeServerTestBundle> {
protected:
    boost::asio::io_context ioContext_;
};

TEST_P(MakeServerTest, Make)
{
    ConfigFileJson const json{boost::json::parse(GetParam().configJson).as_object()};

    util::config::ClioConfigDefinition config{
        {"server.ip", ConfigValue{ConfigType::String}.optional()},
        {"server.port", ConfigValue{ConfigType::Integer}.optional()},
        {"server.processing_policy", ConfigValue{ConfigType::String}.defaultValue("parallel")},
        {"server.parallel_requests_limit", ConfigValue{ConfigType::Integer}.optional()},
        {"server.ws_max_sending_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(1500)},
        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")},
        {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()},
        {"ssl_key_file", ConfigValue{ConfigType::String}.optional()}

    };
    auto const errors = config.parse(json);
    ASSERT_TRUE(!errors.has_value());

    auto const expectedServer =
        makeServer(config, [](auto&&) -> std::expected<void, Response> { return {}; }, [](auto&&) {}, ioContext_);
    EXPECT_EQ(expectedServer.has_value(), GetParam().expectSuccess);
}

INSTANTIATE_TEST_CASE_P(
    MakeServerTests,
    MakeServerTest,
    testing::Values(
        MakeServerTestBundle{
            "BadEndpoint",
            R"json(
                {
                    "server": {"ip": "wrong", "port": 12345}
                }
            )json",
            false
        },
        MakeServerTestBundle{
            "BadSslConfig",
            R"json(
        {
            "server": {"ip": "127.0.0.1", "port": 12345},
            "ssl_cert_file": "some_file"
        }
            )json",
            false
        },
        MakeServerTestBundle{
            "BadProcessingPolicy",
            R"json(
        {
            "server": {"ip": "127.0.0.1", "port": 12345, "processing_policy": "wrong"}
        }
            )json",
            false
        },
        MakeServerTestBundle{
            "CorrectConfig_ParallelPolicy",
            R"json(
        {
            "server": {"ip": "127.0.0.1", "port": 12345, "processing_policy": "parallel"}
        }
            )json",
            true
        },
        MakeServerTestBundle{
            "CorrectConfig_SequentPolicy",
            R"json(
        {
            "server": {"ip": "127.0.0.1", "port": 12345, "processing_policy": "sequent"}
        }
            )json",
            true
        }
    ),
    tests::util::kNAME_GENERATOR
);

struct ServerTest : SyncAsioContextTest {
    ServerTest()
    {
        [&]() { ASSERT_TRUE(server_.has_value()); }();
        server_->onGet("/", getHandler_.AsStdFunction());
        server_->onPost("/", postHandler_.AsStdFunction());
        server_->onWs(wsHandler_.AsStdFunction());
    }

protected:
    uint32_t const serverPort_ = tests::util::generateFreePort();

    ClioConfigDefinition const config_{
        {"server.ip", ConfigValue{ConfigType::String}.defaultValue("127.0.0.1").withConstraint(gValidateIp)},
        {"server.port", ConfigValue{ConfigType::Integer}.defaultValue(serverPort_).withConstraint(gValidatePort)},
        {"server.processing_policy", ConfigValue{ConfigType::String}.defaultValue("parallel")},
        {"server.admin_password", ConfigValue{ConfigType::String}.optional()},
        {"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
        {"server.parallel_requests_limit", ConfigValue{ConfigType::Integer}.optional()},
        {"server.ws_max_sending_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(1500)},
        {"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")},
        {"ssl_key_file", ConfigValue{ConfigType::String}.optional()},
        {"ssl_cert_file", ConfigValue{ConfigType::String}.optional()}
    };

    Server::OnConnectCheck emptyOnConnectCheck_ = [](auto&&) -> std::expected<void, Response> { return {}; };
    std::expected<Server, std::string> server_ = makeServer(config_, emptyOnConnectCheck_, [](auto&&) {}, ctx_);

    std::string requestMessage_ = "some request";
    std::string const headerName_ = "Some-header";
    std::string const headerValue_ = "some value";

    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        getHandler_;
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        postHandler_;
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        wsHandler_;
};

TEST_F(ServerTest, BadEndpoint)
{
    boost::asio::ip::tcp::endpoint const endpoint{boost::asio::ip::address_v4::from_string("1.2.3.4"), 0};
    util::TagDecoratorFactory const tagDecoratorFactory{
        ClioConfigDefinition{{"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")}}
    };
    Server server{
        ctx_,
        endpoint,
        std::nullopt,
        ProcessingPolicy::Sequential,
        std::nullopt,
        tagDecoratorFactory,
        std::nullopt,
        emptyOnConnectCheck_,
        [](auto&&) {}
    };

    auto maybeError = server.run();
    ASSERT_TRUE(maybeError.has_value());
    EXPECT_THAT(*maybeError, testing::HasSubstr("Error creating TCP acceptor"));
}

struct ServerHttpTestBundle {
    std::string testName;
    http::verb method;

    Request::Method
    expectedMethod() const
    {
        switch (method) {
            case http::verb::get:
                return Request::Method::Get;
            case http::verb::post:
                return Request::Method::Post;
            default:
                return Request::Method::Unsupported;
        }
    }
};

struct ServerHttpTest : ServerTest, testing::WithParamInterface<ServerHttpTestBundle> {};

TEST_F(ServerHttpTest, ClientDisconnects)
{
    HttpAsyncClient client{ctx_};
    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort_), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        client.disconnect();
        ctx_.stop();
    });

    server_->run();
    runContext();
}

TEST_F(ServerHttpTest, OnConnectCheck)
{
    auto const serverPort = tests::util::generateFreePort();
    boost::asio::ip::tcp::endpoint const endpoint{boost::asio::ip::address_v4::from_string("0.0.0.0"), serverPort};
    util::TagDecoratorFactory const tagDecoratorFactory{
        ClioConfigDefinition{{"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")}}
    };

    testing::StrictMock<testing::MockFunction<std::expected<void, Response>(Connection const&)>> onConnectCheck;

    Server server{
        ctx_,
        endpoint,
        std::nullopt,
        ProcessingPolicy::Sequential,
        std::nullopt,
        tagDecoratorFactory,
        std::nullopt,
        onConnectCheck.AsStdFunction(),
        [](auto&&) {}
    };

    HttpAsyncClient client{ctx_};

    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        boost::asio::steady_timer timer{yield.get_executor()};

        EXPECT_CALL(onConnectCheck, Call)
            .WillOnce([&timer](Connection const& connection) -> std::expected<void, Response> {
                EXPECT_EQ(connection.ip(), "127.0.0.1");
                timer.cancel();
                return {};
            });

        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        // Have to send a request here because the server does async_detect_ssl() which waits for some data to appear
        client.send(
            http::request<http::string_body>{http::verb::get, "/", 11, requestMessage_},
            yield,
            std::chrono::milliseconds{100}
        );

        // Wait for the onConnectCheck to be called
        timer.expires_after(std::chrono::milliseconds{100});
        boost::system::error_code error;  // Unused
        timer.async_wait(yield[error]);

        client.gracefulShutdown();
        ctx_.stop();
    });

    server.run();

    runContext();
}

TEST_F(ServerHttpTest, OnConnectCheckFailed)
{
    auto const serverPort = tests::util::generateFreePort();
    boost::asio::ip::tcp::endpoint const endpoint{boost::asio::ip::address_v4::from_string("0.0.0.0"), serverPort};
    util::TagDecoratorFactory const tagDecoratorFactory{
        ClioConfigDefinition{{"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")}}
    };

    testing::StrictMock<testing::MockFunction<std::expected<void, Response>(Connection const&)>> onConnectCheck;

    Server server{
        ctx_,
        endpoint,
        std::nullopt,
        ProcessingPolicy::Sequential,
        std::nullopt,
        tagDecoratorFactory,
        std::nullopt,
        onConnectCheck.AsStdFunction(),
        [](auto&&) {}
    };

    HttpAsyncClient client{ctx_};

    EXPECT_CALL(onConnectCheck, Call).WillOnce([](Connection const& connection) {
        EXPECT_EQ(connection.ip(), "127.0.0.1");
        return std::unexpected{
            Response{http::status::too_many_requests, boost::json::object{{"error", "some error"}}, connection}
        };
    });

    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        // Have to send a request here because the server does async_detect_ssl() which waits for some data to appear
        client.send(
            http::request<http::string_body>{http::verb::get, "/", 11, requestMessage_},
            yield,
            std::chrono::milliseconds{100}
        );

        auto const response = client.receive(yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(response.has_value()) << response.error().message(); }();
        EXPECT_EQ(response->result(), http::status::too_many_requests);
        EXPECT_EQ(response->body(), R"json({"error":"some error"})json");
        EXPECT_EQ(response->version(), 11);

        client.gracefulShutdown();
        ctx_.stop();
    });

    server.run();

    runContext();
}

TEST_F(ServerHttpTest, OnDisconnectHook)
{
    auto const serverPort = tests::util::generateFreePort();
    boost::asio::ip::tcp::endpoint const endpoint{boost::asio::ip::address_v4::from_string("0.0.0.0"), serverPort};
    util::TagDecoratorFactory const tagDecoratorFactory{
        ClioConfigDefinition{{"log_tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")}}
    };

    testing::StrictMock<testing::MockFunction<void(Connection const&)>> onDisconnectHookMock;

    Server server{
        ctx_,
        endpoint,
        std::nullopt,
        ProcessingPolicy::Sequential,
        std::nullopt,
        tagDecoratorFactory,
        std::nullopt,
        emptyOnConnectCheck_,
        onDisconnectHookMock.AsStdFunction()
    };

    HttpAsyncClient client{ctx_};

    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        boost::asio::steady_timer timer{ctx_.get_executor(), std::chrono::milliseconds{100}};

        EXPECT_CALL(onDisconnectHookMock, Call).WillOnce([&timer](auto&&) { timer.cancel(); });

        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        client.send(
            http::request<http::string_body>{http::verb::get, "/", 11, requestMessage_},
            yield,
            std::chrono::milliseconds{100}
        );

        client.gracefulShutdown();

        // Wait for OnDisconnectHook is called
        boost::system::error_code error;
        timer.async_wait(yield[error]);

        ctx_.stop();
    });

    server.run();

    runContext();
}

TEST_P(ServerHttpTest, RequestResponse)
{
    HttpAsyncClient client{ctx_};

    http::request<http::string_body> request{GetParam().method, "/", 11, requestMessage_};
    request.set(headerName_, headerValue_);

    Response const response{http::status::ok, "some response", Request{request}};

    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort_), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            maybeError = client.send(request, yield, std::chrono::milliseconds{100});
            EXPECT_FALSE(maybeError.has_value()) << maybeError->message();

            auto const expectedResponse = client.receive(yield, std::chrono::milliseconds{100});
            [&]() { ASSERT_TRUE(expectedResponse.has_value()) << expectedResponse.error().message(); }();
            EXPECT_EQ(expectedResponse->result(), http::status::ok);
            EXPECT_EQ(expectedResponse->body(), response.message());
        }

        client.gracefulShutdown();
        ctx_.stop();
    });

    auto& handler = GetParam().method == http::verb::get ? getHandler_ : postHandler_;

    EXPECT_CALL(handler, Call)
        .Times(3)
        .WillRepeatedly([&, response = response](Request const& receivedRequest, auto&&, auto&&, auto&&) {
            EXPECT_TRUE(receivedRequest.isHttp());
            EXPECT_EQ(receivedRequest.method(), GetParam().expectedMethod());
            EXPECT_EQ(receivedRequest.message(), request.body());
            EXPECT_EQ(receivedRequest.target(), request.target());
            EXPECT_EQ(receivedRequest.headerValue(headerName_), request.at(headerName_));

            return response;
        });

    server_->run();

    runContext();
}

INSTANTIATE_TEST_SUITE_P(
    ServerHttpTests,
    ServerHttpTest,
    testing::Values(ServerHttpTestBundle{"GET", http::verb::get}, ServerHttpTestBundle{"POST", http::verb::post}),
    tests::util::kNAME_GENERATOR
);

TEST_F(ServerTest, WsClientDisconnects)
{
    WebSocketAsyncClient client{ctx_};

    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort_), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        client.close();
        ctx_.stop();
    });

    server_->run();

    runContext();
}

TEST_F(ServerTest, WsRequestResponse)
{
    WebSocketAsyncClient client{ctx_};

    Request::HttpHeaders const headers{};
    Response const response{http::status::ok, "some response", Request{requestMessage_, headers}};

    boost::asio::spawn(ctx_, [&](boost::asio::yield_context yield) {
        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort_), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            maybeError = client.send(yield, requestMessage_, std::chrono::milliseconds{100});
            EXPECT_FALSE(maybeError.has_value()) << maybeError->message();

            auto const expectedResponse = client.receive(yield, std::chrono::milliseconds{100});
            [&]() { ASSERT_TRUE(expectedResponse.has_value()) << expectedResponse.error().message(); }();
            EXPECT_EQ(expectedResponse.value(), response.message());
        }

        client.gracefulClose(yield, std::chrono::milliseconds{100});
        ctx_.stop();
    });

    EXPECT_CALL(wsHandler_, Call)
        .Times(3)
        .WillRepeatedly([&, response = response](Request const& receivedRequest, auto&&, auto&&, auto&&) {
            EXPECT_FALSE(receivedRequest.isHttp());
            EXPECT_EQ(receivedRequest.method(), Request::Method::Websocket);
            EXPECT_EQ(receivedRequest.message(), requestMessage_);
            EXPECT_EQ(receivedRequest.target(), std::nullopt);

            return response;
        });

    server_->run();

    runContext();
}
