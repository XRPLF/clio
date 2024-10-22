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
#include "util/config/Config.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/Server.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
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
#include <utility>

using namespace web::ng;

namespace http = boost::beast::http;

struct MakeServerTestBundle {
    std::string testName;
    std::string configJson;
    bool expectSuccess;
};

struct MakeServerTest : NoLoggerFixture, testing::WithParamInterface<MakeServerTestBundle> {
    boost::asio::io_context ioContext_;
};

TEST_P(MakeServerTest, Make)
{
    util::Config const config{boost::json::parse(GetParam().configJson)};
    auto const expectedServer = make_Server(config, ioContext_);
    EXPECT_EQ(expectedServer.has_value(), GetParam().expectSuccess);
}

INSTANTIATE_TEST_CASE_P(
    MakeServerTests,
    MakeServerTest,
    testing::Values(
        MakeServerTestBundle{
            "NoIp",
            R"json(
                {
                    "server": {"port": 12345}
                }
            )json",
            false
        },
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
            "PortMissing",
            R"json(
        {
            "server": {"ip": "127.0.0.1"}
        }
            )json",
            false
        },
        MakeServerTestBundle{
            "BadSslConfig",
            R"json(
        {
            "server": {"ip": "127.0.0.1", "port": 12345},
            "ssl_cert_file": "somг_file"
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
    tests::util::NameGenerator
);

struct ServerTest : SyncAsioContextTest {
    ServerTest()
    {
        [&]() { ASSERT_TRUE(server_.has_value()); }();
        server_->onGet("/", getHandler_.AsStdFunction());
        server_->onPost("/", postHandler_.AsStdFunction());
        server_->onWs(wsHandler_.AsStdFunction());
    }

    uint32_t const serverPort_ = tests::util::generateFreePort();

    util::Config const config_{
        boost::json::object{{"server", boost::json::object{{"ip", "127.0.0.1"}, {"port", serverPort_}}}}
    };

    std::expected<Server, std::string> server_ = make_Server(config_, ctx);

    std::string requestMessage_ = "some request";
    std::string const headerName_ = "Some-header";
    std::string const headerValue_ = "some value";

    testing::StrictMock<testing::MockFunction<Response(Request const&, ConnectionContext, boost::asio::yield_context)>>
        getHandler_;
    testing::StrictMock<testing::MockFunction<Response(Request const&, ConnectionContext, boost::asio::yield_context)>>
        postHandler_;
    testing::StrictMock<testing::MockFunction<Response(Request const&, ConnectionContext, boost::asio::yield_context)>>
        wsHandler_;
};

TEST_F(ServerTest, BadEndpoint)
{
    boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::address_v4::from_string("1.2.3.4"), 0};
    impl::ConnectionHandler connectionHandler{impl::ConnectionHandler::ProcessingPolicy::Sequential, std::nullopt};
    util::TagDecoratorFactory tagDecoratorFactory{util::Config{boost::json::value{}}};
    Server server{ctx, endpoint, std::nullopt, std::move(connectionHandler), tagDecoratorFactory};
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
                return Request::Method::GET;
            case http::verb::post:
                return Request::Method::POST;
            default:
                return Request::Method::UNSUPPORTED;
        }
    }
};

struct ServerHttpTest : ServerTest, testing::WithParamInterface<ServerHttpTestBundle> {};

TEST_F(ServerHttpTest, ClientDisconnects)
{
    HttpAsyncClient client{ctx};
    boost::asio::spawn(ctx, [&](boost::asio::yield_context yield) {
        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort_), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        client.disconnect();
        ctx.stop();
    });

    server_->run();
    runContext();
}

TEST_P(ServerHttpTest, RequestResponse)
{
    HttpAsyncClient client{ctx};

    http::request<http::string_body> request{GetParam().method, "/", 11, requestMessage_};
    request.set(headerName_, headerValue_);

    Response const response{http::status::ok, "some response", Request{request}};

    boost::asio::spawn(ctx, [&](boost::asio::yield_context yield) {
        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort_), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        for ([[maybe_unused]] auto _i : std::ranges::iota_view{0, 3}) {
            maybeError = client.send(request, yield, std::chrono::milliseconds{100});
            EXPECT_FALSE(maybeError.has_value()) << maybeError->message();

            auto const expectedResponse = client.receive(yield, std::chrono::milliseconds{100});
            [&]() { ASSERT_TRUE(expectedResponse.has_value()) << expectedResponse.error().message(); }();
            EXPECT_EQ(expectedResponse->result(), http::status::ok);
            EXPECT_EQ(expectedResponse->body(), response.message());
        }

        client.gracefulShutdown();
        ctx.stop();
    });

    auto& handler = GetParam().method == http::verb::get ? getHandler_ : postHandler_;

    EXPECT_CALL(handler, Call)
        .Times(3)
        .WillRepeatedly([&, response = response](Request const& receivedRequest, auto&&, auto&&) {
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
    tests::util::NameGenerator
);

TEST_F(ServerTest, WsClientDisconnects)
{
    WebSocketAsyncClient client{ctx};

    boost::asio::spawn(ctx, [&](boost::asio::yield_context yield) {
        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort_), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        client.close();
        ctx.stop();
    });

    server_->run();

    runContext();
}

TEST_F(ServerTest, WsRequestResponse)
{
    WebSocketAsyncClient client{ctx};

    Response const response{http::status::ok, "some response", Request{requestMessage_, Request::HttpHeaders{}}};

    boost::asio::spawn(ctx, [&](boost::asio::yield_context yield) {
        auto maybeError =
            client.connect("127.0.0.1", std::to_string(serverPort_), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError->message(); }();

        for ([[maybe_unused]] auto _i : std::ranges::iota_view{0, 3}) {
            maybeError = client.send(yield, requestMessage_, std::chrono::milliseconds{100});
            EXPECT_FALSE(maybeError.has_value()) << maybeError->message();

            auto const expectedResponse = client.receive(yield, std::chrono::milliseconds{100});
            [&]() { ASSERT_TRUE(expectedResponse.has_value()) << expectedResponse.error().message(); }();
            EXPECT_EQ(expectedResponse.value(), response.message());
        }

        client.gracefulClose(yield, std::chrono::milliseconds{100});
        ctx.stop();
    });

    EXPECT_CALL(wsHandler_, Call)
        .Times(3)
        .WillRepeatedly([&, response = response](Request const& receivedRequest, auto&&, auto&&) {
            EXPECT_FALSE(receivedRequest.isHttp());
            EXPECT_EQ(receivedRequest.method(), Request::Method::WEBSOCKET);
            EXPECT_EQ(receivedRequest.message(), requestMessage_);
            EXPECT_EQ(receivedRequest.target(), std::nullopt);

            return response;
        });

    server_->run();

    runContext();
}
