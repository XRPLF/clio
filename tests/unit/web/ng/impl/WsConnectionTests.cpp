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
#include "util/CoroutineGroup.hpp"
#include "util/Taggable.hpp"
#include "util/TestHttpServer.hpp"
#include "util/TestWebSocketClient.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/HttpConnection.hpp"
#include "web/ng/impl/WsConnection.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/status.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <thread>
#include <utility>

using namespace web::ng::impl;
using namespace web::ng;
using namespace util;

struct WebWsConnectionTests : SyncAsioContextTest {
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
        wsConnectionPtr->setTimeout(std::chrono::milliseconds{100});
        return std::unique_ptr<PlainWsConnection>{wsConnectionPtr};
    }

protected:
    util::TagDecoratorFactory tagDecoratorFactory_{config::ClioConfigDefinition{
        {"log_tag_style", config::ConfigValue{config::ConfigType::String}.defaultValue("int")}
    }};
    TestHttpServer httpServer_{ctx_, "localhost"};
    WebSocketAsyncClient wsClient_{ctx_};
    Request::HttpHeaders const headers_;
    Request request_{"some request", headers_};
};

TEST_F(WebWsConnectionTests, WasUpgraded)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
    });
    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        EXPECT_TRUE(wsConnection->wasUpgraded());
    });
}

TEST_F(WebWsConnectionTests, DisconnectClientOnInactivity)
{
    boost::asio::io_context clientCtx;
    auto work = boost::asio::make_work_guard(clientCtx);
    std::thread clientThread{[&clientCtx]() { clientCtx.run(); }};

    boost::asio::spawn(clientCtx, [&work, this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        boost::asio::steady_timer timer{yield.get_executor(), std::chrono::milliseconds{5}};
        timer.async_wait(yield);
        work.reset();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        wsConnection->setTimeout(std::chrono::milliseconds{1});
        // Client will not respond to pings because there is no reading operation scheduled for it.

        auto const start = std::chrono::steady_clock::now();
        auto const receivedMessage = wsConnection->receive(yield);
        auto const end = std::chrono::steady_clock::now();
        EXPECT_LT(end - start, std::chrono::milliseconds{4});  // Should be 2 ms, double it in case of slow CI.

        EXPECT_FALSE(receivedMessage.has_value());
        EXPECT_EQ(receivedMessage.error().value(), boost::asio::error::no_permission);
    });
    clientThread.join();
}

TEST_F(WebWsConnectionTests, Send)
{
    Response const response{boost::beast::http::status::ok, "some response", request_};

    boost::asio::spawn(ctx_, [this, &response](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        auto const expectedMessage = wsClient_.receive(yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedMessage.has_value()) << expectedMessage.error().message(); }();
        EXPECT_EQ(expectedMessage.value(), response.message());
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        auto maybeError = wsConnection->send(response, yield);
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
    });
}

TEST_F(WebWsConnectionTests, MultipleSend)
{
    Response const response{boost::beast::http::status::ok, "some response", request_};

    boost::asio::spawn(ctx_, [this, &response](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            auto const expectedMessage = wsClient_.receive(yield, std::chrono::milliseconds{100});
            [&]() { ASSERT_TRUE(expectedMessage.has_value()) << expectedMessage.error().message(); }();
            EXPECT_EQ(expectedMessage.value(), response.message());
        }
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);

        for ([[maybe_unused]] auto unused : std::ranges::iota_view{0, 3}) {
            auto maybeError = wsConnection->send(response, yield);
            [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        }
    });
}

TEST_F(WebWsConnectionTests, SendFailed)
{
    Response const response{boost::beast::http::status::ok, "some response", request_};

    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        wsClient_.close();
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        wsConnection->setTimeout(std::chrono::milliseconds{1});
        std::optional<Error> maybeError;
        size_t counter = 0;
        while (not maybeError.has_value() and counter < 100) {
            maybeError = wsConnection->send(response, yield);
            ++counter;
        }
        EXPECT_TRUE(maybeError.has_value());
        EXPECT_LT(counter, 100);
    });
}

TEST_F(WebWsConnectionTests, Receive)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();

        maybeError = wsClient_.send(yield, request_.message(), std::chrono::milliseconds{100});
        EXPECT_FALSE(maybeError.has_value()) << maybeError->message();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);

        auto maybeRequest = wsConnection->receive(yield);
        [&]() { ASSERT_TRUE(maybeRequest.has_value()) << maybeRequest.error().message(); }();
        EXPECT_EQ(maybeRequest->message(), request_.message());
    });
}

TEST_F(WebWsConnectionTests, MultipleReceive)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            maybeError = wsClient_.send(yield, request_.message(), std::chrono::milliseconds{100});
            EXPECT_FALSE(maybeError.has_value()) << maybeError->message();
        }
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);

        for ([[maybe_unused]] auto unused : std::ranges::iota_view{0, 3}) {
            auto maybeRequest = wsConnection->receive(yield);
            [&]() { ASSERT_TRUE(maybeRequest.has_value()) << maybeRequest.error().message(); }();
            EXPECT_EQ(maybeRequest->message(), request_.message());
        }
    });
}

TEST_F(WebWsConnectionTests, ReceiveTimeout)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        wsConnection->setTimeout(std::chrono::milliseconds{2});
        auto maybeRequest = wsConnection->receive(yield);
        EXPECT_FALSE(maybeRequest.has_value());
        EXPECT_EQ(maybeRequest.error().value(), boost::system::errc::operation_not_permitted);
    });
}

TEST_F(WebWsConnectionTests, ReceiveFailed)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        wsClient_.close();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        auto maybeRequest = wsConnection->receive(yield);
        EXPECT_FALSE(maybeRequest.has_value());
        EXPECT_EQ(maybeRequest.error().value(), boost::asio::error::eof);
    });
}

TEST_F(WebWsConnectionTests, Close)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        auto const maybeMessage = wsClient_.receive(yield, std::chrono::milliseconds{100});
        EXPECT_FALSE(maybeMessage.has_value());
        EXPECT_THAT(maybeMessage.error().message(), testing::HasSubstr("was gracefully closed"));
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        wsConnection->close(yield);
    });
}

TEST_F(WebWsConnectionTests, CloseWhenConnectionIsAlreadyClosed)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
        wsClient_.close();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        boost::asio::post(yield);
        wsConnection->close(yield);
        wsConnection->close(yield);
    });
}

TEST_F(WebWsConnectionTests, CloseCalledFromMultipleSubCoroutines)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto maybeError = wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_FALSE(maybeError.has_value()) << maybeError.value().message(); }();
    });

    testing::StrictMock<testing::MockFunction<void()>> closeCalled;
    EXPECT_CALL(closeCalled, Call).Times(2);

    runSpawnWithTimeout(std::chrono::seconds{1}, [&](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        util::CoroutineGroup coroutines{yield};
        for ([[maybe_unused]] int const i : std::ranges::iota_view{0, 2}) {
            coroutines.spawn(yield, [&wsConnection, &closeCalled](boost::asio::yield_context innerYield) {
                wsConnection->close(innerYield);
                closeCalled.Call();
            });
        }
        auto const receivedMessage = wsConnection->receive(yield);
        EXPECT_FALSE(receivedMessage.has_value());
        coroutines.asyncWait(yield);
    });
}
