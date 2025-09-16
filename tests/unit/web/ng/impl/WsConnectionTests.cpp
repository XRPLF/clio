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
#include "util/Spawn.hpp"
#include "util/Taggable.hpp"
#include "util/TestHttpServer.hpp"
#include "util/TestWebSocketClient.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
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
            std::move(expectedSocket).value(),
            std::move(ip),
            boost::beast::flat_buffer{},
            tagDecoratorFactory_,
        };

        auto expectedTrue = httpConnection.isUpgradeRequested(yield);
        [&]() {
            ASSERT_TRUE(expectedTrue.has_value()) << expectedTrue.error().message();
            ASSERT_TRUE(expectedTrue.value()) << "Expected upgrade request";
        }();

        auto expectedWsConnection = httpConnection.upgrade(tagDecoratorFactory_, yield);
        [&]() { ASSERT_TRUE(expectedWsConnection.has_value()) << expectedWsConnection.error().message(); }();
        auto connection = std::move(expectedWsConnection).value();
        auto wsConnectionPtr = dynamic_cast<PlainWsConnection*>(connection.release());
        [&]() { ASSERT_NE(wsConnectionPtr, nullptr) << "Expected PlainWsConnection"; }();
        wsConnectionPtr->setTimeout(std::chrono::milliseconds{100});
        return std::unique_ptr<PlainWsConnection>{wsConnectionPtr};
    }

protected:
    util::TagDecoratorFactory tagDecoratorFactory_{config::ClioConfigDefinition{
        {"log.tag_style", config::ConfigValue{config::ConfigType::String}.defaultValue("int")}
    }};
    TestHttpServer httpServer_{ctx_, "localhost"};
    WebSocketAsyncClient wsClient_{ctx_};
    Request::HttpHeaders const headers_;
    Request request_{"some request", headers_};
};

TEST_F(WebWsConnectionTests, WasUpgraded)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
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

    util::spawn(clientCtx, [&work, this](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
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

    util::spawn(ctx_, [this, &response](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
        auto const expectedMessage = wsClient_.receive(yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedMessage.has_value()) << expectedMessage.error().message(); }();
        EXPECT_EQ(expectedMessage.value(), response.message());
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        auto expectedSuccess = wsConnection->send(response, yield);
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
    });
}

TEST_F(WebWsConnectionTests, SendShared)
{
    auto const response = std::make_shared<std::string>("some response");

    util::spawn(ctx_, [this, &response](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
        auto const expectedMessage = wsClient_.receive(yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedMessage.has_value()) << expectedMessage.error().message(); }();
        EXPECT_EQ(expectedMessage.value(), *response);
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        auto expectedSuccess = wsConnection->sendShared(response, yield);
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
    });
}

TEST_F(WebWsConnectionTests, MultipleSend)
{
    Response const response{boost::beast::http::status::ok, "some response", request_};

    util::spawn(ctx_, [this, &response](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            auto const expectedMessage = wsClient_.receive(yield, std::chrono::milliseconds{100});
            [&]() { ASSERT_TRUE(expectedMessage.has_value()) << expectedMessage.error().message(); }();
            EXPECT_EQ(expectedMessage.value(), response.message());
        }
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            auto expectedSuccess = wsConnection->send(response, yield);
            [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
        }
    });
}

TEST_F(WebWsConnectionTests, MultipleSendFromMultipleCoroutines)
{
    Response const response{boost::beast::http::status::ok, "some response", request_};

    util::spawn(ctx_, [this, &response](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            auto const expectedMessage = wsClient_.receive(yield, std::chrono::milliseconds{100});
            [&]() { ASSERT_TRUE(expectedMessage.has_value()) << expectedMessage.error().message(); }();
            EXPECT_EQ(expectedMessage.value(), response.message());
        }
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);

        util::CoroutineGroup group{yield};
        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            group.spawn(yield, [&wsConnection, &response](boost::asio::yield_context innerYield) {
                auto expectedSuccess = wsConnection->send(response, innerYield);
                [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
            });
        }
        group.asyncWait(yield);
    });
}

TEST_F(WebWsConnectionTests, SendFailed)
{
    Response const response{boost::beast::http::status::ok, "some response", request_};

    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
        wsClient_.close();
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        wsConnection->setTimeout(std::chrono::milliseconds{1});
        std::expected<void, Error> expectedSuccess;
        size_t counter = 0;
        while (expectedSuccess.has_value() and counter < 100) {
            expectedSuccess = wsConnection->send(response, yield);
            ++counter;
        }
        EXPECT_FALSE(expectedSuccess.has_value());
        EXPECT_LT(counter, 100);
    });
}

TEST_F(WebWsConnectionTests, SendFailedSendingFromMultipleCoroutines)
{
    Response const response{boost::beast::http::status::ok, "some response", request_};

    util::spawn(ctx_, [this, &response](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();

        auto const expectedMessage = wsClient_.receive(yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedMessage.has_value()) << expectedMessage.error().message(); }();
        EXPECT_EQ(expectedMessage.value(), response.message());
        wsClient_.close();
    });

    runSpawn([this, &response](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);
        wsConnection->setTimeout(std::chrono::milliseconds{1});
        std::expected<void, Error> expectedSuccess;
        size_t counter = 0;
        while (expectedSuccess.has_value() and counter < 100) {
            expectedSuccess = wsConnection->send(response, yield);
            ++counter;
        }
        // Sending after getting an error should be safe
        expectedSuccess = wsConnection->send(response, yield);
        EXPECT_FALSE(expectedSuccess.has_value());
        EXPECT_LT(counter, 100);
    });
}

TEST_F(WebWsConnectionTests, Receive)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();

        expectedSuccess = wsClient_.send(yield, request_.message(), std::chrono::milliseconds{100});
        EXPECT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message();
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
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            expectedSuccess = wsClient_.send(yield, request_.message(), std::chrono::milliseconds{100});
            EXPECT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message();
        }
    });

    runSpawn([this](boost::asio::yield_context yield) {
        auto wsConnection = acceptConnection(yield);

        for ([[maybe_unused]] auto i : std::ranges::iota_view{0, 3}) {
            auto maybeRequest = wsConnection->receive(yield);
            [&]() { ASSERT_TRUE(maybeRequest.has_value()) << maybeRequest.error().message(); }();
            EXPECT_EQ(maybeRequest->message(), request_.message());
        }
    });
}

TEST_F(WebWsConnectionTests, ReceiveTimeout)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
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
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
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
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
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
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
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
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto expectedSuccess =
            wsClient_.connect("localhost", httpServer_.port(), yield, std::chrono::milliseconds{100});
        [&]() { ASSERT_TRUE(expectedSuccess.has_value()) << expectedSuccess.error().message(); }();
    });

    testing::StrictMock<testing::MockFunction<void()>> closeCalled;
    EXPECT_CALL(closeCalled, Call).Times(2);

    runSpawn([&](boost::asio::yield_context yield) {
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
