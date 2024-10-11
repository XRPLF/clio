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
#include "util/Taggable.hpp"
#include "util/TestHttpServer.hpp"
#include "util/TestWebSocketClient.hpp"
#include "util/config/Config.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/HttpConnection.hpp"
#include "web/ng/impl/WsConnection.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

using namespace web::ng::impl;
using namespace web::ng;

struct web_WsConnectionTests : SyncAsioContextTest {
    util::TagDecoratorFactory tagDecoratorFactory_{util::Config{boost::json::object{{"log_tag_style", "int"}}}};
    TestHttpServer httpServer_{ctx, "localhost"};
    WebSocketAsyncClient wsClient_{ctx};
    Request request_{"some request", Request::HttpHeaders{}};

    std::unique_ptr<PlainWsConnection>
    acceptConnection(boost::asio::yield_context yield)
    {
        auto expectedSocket = httpServer_.accept(yield);
        [&]() { ASSERT_TRUE(expectedSocket.has_value()) << expectedSocket.error().message(); }();
        auto ip = expectedSocket->remote_endpoint().address().to_string();

        PlainHttpConnection httpConnection{
            std::move(expectedSocket).value(), std::move(ip), boost::beast::flat_buffer{}, tagDecoratorFactory_
        };

        auto expectedTrue = httpConnection.isUpgradeRequested(yield);
        [&]() {
            ASSERT_TRUE(expectedTrue.has_value()) << expectedTrue.error().message();
            ASSERT_TRUE(expectedTrue.value()) << "Expected upgrade request";
        }();

        std::optional<boost::asio::ssl::context> sslContext;
        auto expectedWsConnection = httpConnection.upgrade(sslContext, tagDecoratorFactory_, yield);
        [&]() { ASSERT_TRUE(expectedWsConnection.has_value()) << expectedWsConnection.error().message(); }();
        auto connection = std::move(expectedWsConnection).value();
        auto wsConnectionPtr = dynamic_cast<PlainWsConnection*>(connection.release());
        [&]() { ASSERT_NE(wsConnectionPtr, nullptr) << "Expected PlainWsConnection"; }();
        return std::unique_ptr<PlainWsConnection>{wsConnectionPtr};
    }
};

TEST_F(web_WsConnectionTests, WasUpgraded)
{
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
    });
    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        EXPECT_TRUE(wsConnection->wasUpgraded());
    });
}

TEST_F(web_WsConnectionTests, Send)
{
    Response const response{boost::beast::http::status::ok, "some response", request_};

    boost::asio::spawn(ctx, [this, &response](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        auto const expectedMessage = wsClient_.receive(yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedMessage.has_value()) << expectedMessage.error().message(); }();
        EXPECT_EQ(expectedMessage.value(), response.message());
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        auto maybeError = wsConnection->send(response, yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
    });
}

TEST_F(web_WsConnectionTests, SendFailed)
{
    Response const response{boost::beast::http::status::ok, "some response", request_};

    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        wsClient_.close();
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        std::optional<Error> maybeError;
        size_t counter = 0;
        while (not maybeError.has_value() and counter < 100) {
            maybeError = wsConnection->send(response, yield, std::chrono::milliseconds{1});
            ++counter;
        }
        EXPECT_TRUE(maybeError.has_value());
        EXPECT_LT(counter, 100);
    });
}

TEST_F(web_WsConnectionTests, Receive)
{
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        wsClient_.send(yield, request_.message(), std::chrono::milliseconds{100});
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        auto maybeRequest = wsConnection->receive(yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(maybeRequest.has_value()) << maybeRequest.error().message(); }();
        EXPECT_EQ(maybeRequest->message(), request_.message());
    });
}

TEST_F(web_WsConnectionTests, ReceiveTimeout)
{
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        auto maybeRequest = wsConnection->receive(yield, std::chrono::milliseconds{1});
        EXPECT_FALSE(maybeRequest.has_value());
        EXPECT_EQ(maybeRequest.error().value(), boost::asio::error::timed_out);
    });
}

TEST_F(web_WsConnectionTests, ReceiveFailed)
{
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        wsClient_.close();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        auto maybeRequest = wsConnection->receive(yield, std::chrono::milliseconds{100});
        EXPECT_FALSE(maybeRequest.has_value());
        EXPECT_EQ(maybeRequest.error().value(), boost::asio::error::eof);
    });
}

TEST_F(web_WsConnectionTests, Close)
{
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        auto const maybeMessage = wsClient_.receive(yield, std::chrono::milliseconds{100});
        EXPECT_FALSE(maybeMessage.has_value());
        EXPECT_THAT(maybeMessage.error().message(), testing::HasSubstr("was gracefully closed"));
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        wsConnection->close(yield, std::chrono::milliseconds{100});
    });
}
