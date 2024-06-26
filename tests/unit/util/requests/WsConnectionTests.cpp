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

#include "util/Fixtures.hpp"
#include "util/TestWsServer.hpp"
#include "util/requests/Types.hpp"
#include "util/requests/WsConnection.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/http/field.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace util::requests;
namespace asio = boost::asio;
namespace http = boost::beast::http;

struct WsConnectionTestsBase : SyncAsioContextTest {
    TestWsServer server{ctx, "0.0.0.0"};
    WsConnectionBuilder builder{"localhost", server.port()};

    template <typename T, typename E>
    T
    unwrap(std::expected<T, E> expected)
    {
        [&]() { ASSERT_TRUE(expected.has_value()) << expected.error().message(); }();
        return std::move(expected).value();
    }
};

struct WsConnectionTestBundle {
    std::string testName;
    std::vector<HttpHeader> headers;
    std::optional<std::string> target;
};

struct WsConnectionTests : WsConnectionTestsBase, testing::WithParamInterface<WsConnectionTestBundle> {
    WsConnectionTests()
    {
        [this]() { ASSERT_EQ(clientMessages.size(), serverMessages.size()); }();
    }
    std::vector<std::string> const clientMessages{"hello", "world"};

    std::vector<std::string> const serverMessages{"goodbye", "point"};
};

INSTANTIATE_TEST_CASE_P(
    WsConnectionTestsGroup,
    WsConnectionTests,
    testing::Values(
        WsConnectionTestBundle{"noHeaders", {}, std::nullopt},
        WsConnectionTestBundle{"singleHeader", {{http::field::accept, "text/html"}}, std::nullopt},
        WsConnectionTestBundle{
            "multiple headers",
            {{http::field::accept, "text/html"},
             {http::field::authorization, "password"},
             {"Custom_header", "some_value"}},
            std::nullopt
        },
        WsConnectionTestBundle{"target", {}, "/target"}
    )
);

TEST_P(WsConnectionTests, SendAndReceive)
{
    if (auto const target = GetParam().target; target) {
        builder.setTarget(*target);
    }
    builder.addHeaders(GetParam().headers);

    asio::spawn(ctx, [&](asio::yield_context yield) {
        auto serverConnection = unwrap(server.acceptConnection(yield));

        for (size_t i = 0; i < clientMessages.size(); ++i) {
            auto message = serverConnection.receive(yield);
            EXPECT_EQ(clientMessages.at(i), message);

            auto error = serverConnection.send(serverMessages.at(i), yield);
            ASSERT_FALSE(error) << *error;
        }
    });

    runSpawn([&](asio::yield_context yield) {
        auto maybeConnection = builder.plainConnect(yield);
        ASSERT_TRUE(maybeConnection.has_value()) << maybeConnection.error().message();
        auto& connection = *maybeConnection;

        for (size_t i = 0; i < serverMessages.size(); ++i) {
            auto error = connection->write(clientMessages.at(i), yield);
            ASSERT_FALSE(error) << error->message();

            auto message = connection->read(yield);
            ASSERT_TRUE(message.has_value()) << message.error().message();
            EXPECT_EQ(serverMessages.at(i), message.value());
        }
    });
}

TEST_F(WsConnectionTests, ReadTimeout)
{
    TestWsConnectionPtr serverConnection;
    asio::spawn(ctx, [&](asio::yield_context yield) {
        serverConnection = std::make_unique<TestWsConnection>(unwrap(server.acceptConnection(yield)));
    });

    runSpawn([&](asio::yield_context yield) {
        auto connection = unwrap(builder.plainConnect(yield));
        auto message = connection->read(yield, std::chrono::milliseconds{1});
        ASSERT_FALSE(message.has_value());
        ASSERT_TRUE(message.error().errorCode().has_value());
        EXPECT_EQ(message.error().errorCode().value().value(), asio::error::timed_out);
    });
}

TEST_F(WsConnectionTests, ReadWithTimeoutWorksFine)
{
    asio::spawn(ctx, [&](asio::yield_context yield) {
        auto serverConnection = unwrap(server.acceptConnection(yield));
        auto maybeError = serverConnection.send("hello", yield);
        EXPECT_FALSE(maybeError.has_value()) << *maybeError;
    });

    runSpawn([&](asio::yield_context yield) {
        auto connection = unwrap(builder.plainConnect(yield));
        auto message = connection->read(yield, std::chrono::seconds{1});
        ASSERT_TRUE(message.has_value()) << message.error().message();
        EXPECT_EQ(message.value(), "hello");
    });
}

TEST_F(WsConnectionTests, WriteTimeout)
{
    TestWsConnectionPtr serverConnection;
    asio::spawn(ctx, [&](asio::yield_context yield) {
        serverConnection = std::make_unique<TestWsConnection>(unwrap(server.acceptConnection(yield)));
    });

    runSpawn([&](asio::yield_context yield) {
        auto connection = unwrap(builder.plainConnect(yield));
        std::optional<RequestError> error;

        // Write is success even if the other side is not reading.
        // It seems we need to fill some socket buffer before the timeout occurs.
        while (not error.has_value()) {
            error = connection->write("hello", yield, std::chrono::milliseconds{1});
        }
        ASSERT_TRUE(error.has_value());
        EXPECT_EQ(error->errorCode().value().value(), asio::error::timed_out);
    });
}

TEST_F(WsConnectionTests, WriteWithTimeoutWorksFine)
{
    asio::spawn(ctx, [&](asio::yield_context yield) {
        auto serverConnection = unwrap(server.acceptConnection(yield));
        auto message = serverConnection.receive(yield);
        ASSERT_TRUE(message.has_value());
        EXPECT_EQ(message, "hello");
    });

    runSpawn([&](asio::yield_context yield) {
        auto connection = unwrap(builder.plainConnect(yield));
        auto error = connection->write("hello", yield, std::chrono::seconds{1});
        ASSERT_FALSE(error.has_value()) << error->message();
    });
}

TEST_F(WsConnectionTests, TrySslUsePlain)
{
    asio::spawn(ctx, [&](asio::yield_context yield) {
        // Client attempts to establish SSL connection first which will fail
        auto failedConnection = server.acceptConnection(yield);
        EXPECT_FALSE(failedConnection.has_value());

        auto serverConnection = unwrap(server.acceptConnection(yield));
        auto message = serverConnection.receive(yield);
        EXPECT_EQ(message, "hello");

        auto error = serverConnection.send("goodbye", yield);
        EXPECT_FALSE(error) << *error;
    });

    runSpawn([&](asio::yield_context yield) {
        auto maybeConnection = builder.connect(yield);
        ASSERT_TRUE(maybeConnection.has_value()) << maybeConnection.error().message();
        auto& connection = *maybeConnection;

        auto error = connection->write("hello", yield);
        ASSERT_FALSE(error) << error->message();

        auto message = connection->read(yield);
        ASSERT_TRUE(message.has_value()) << message.error().message();
        EXPECT_EQ(message.value(), "goodbye");
    });
}

TEST_F(WsConnectionTests, ConnectionTimeout)
{
    builder.setConnectionTimeout(std::chrono::milliseconds{1});
    runSpawn([&](asio::yield_context yield) {
        auto connection = builder.plainConnect(yield);
        ASSERT_FALSE(connection.has_value());

        EXPECT_TRUE(connection.error().message().starts_with("Connect error"));
    });
}

TEST_F(WsConnectionTests, ResolveError)
{
    builder = WsConnectionBuilder{"wrong_host", "11112"};
    runSpawn([&](asio::yield_context yield) {
        auto connection = builder.plainConnect(yield);
        ASSERT_FALSE(connection.has_value());
        EXPECT_TRUE(connection.error().message().starts_with("Resolve error")) << connection.error().message();
    });
}

TEST_F(WsConnectionTests, WsHandshakeError)
{
    builder.setConnectionTimeout(std::chrono::milliseconds{1});
    asio::spawn(ctx, [&](asio::yield_context yield) { server.acceptConnectionAndDropIt(yield); });
    runSpawn([&](asio::yield_context yield) {
        auto connection = builder.plainConnect(yield);
        ASSERT_FALSE(connection.has_value());
        EXPECT_TRUE(connection.error().message().starts_with("Handshake error")) << connection.error().message();
    });
}

TEST_F(WsConnectionTests, WsHandshakeTimeout)
{
    builder.setWsHandshakeTimeout(std::chrono::milliseconds{1});
    asio::spawn(ctx, [&](asio::yield_context yield) {
        auto socket = server.acceptConnectionWithoutHandshake(yield);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    });
    runSpawn([&](asio::yield_context yield) {
        auto connection = builder.plainConnect(yield);
        ASSERT_FALSE(connection.has_value());
        EXPECT_TRUE(connection.error().message().starts_with("Handshake error")) << connection.error().message();
    });
}

TEST_F(WsConnectionTests, CloseConnection)
{
    asio::spawn(ctx, [&](asio::yield_context yield) {
        auto serverConnection = unwrap(server.acceptConnection(yield));

        auto message = serverConnection.receive(yield);
        EXPECT_EQ(std::nullopt, message);
    });

    runSpawn([&](asio::yield_context yield) {
        auto connection = unwrap(builder.plainConnect(yield));

        auto error = connection->close(yield);
        EXPECT_FALSE(error.has_value()) << error->message();
    });
}

TEST_F(WsConnectionTests, CloseConnectionTimeout)
{
    TestWsConnectionPtr const serverConnection;
    asio::spawn(ctx, [&](asio::yield_context yield) {
        auto serverConnection = std::make_unique<TestWsConnection>(unwrap(server.acceptConnection(yield)));
    });

    runSpawn([&](asio::yield_context yield) {
        auto connection = unwrap(builder.plainConnect(yield));

        auto error = connection->close(yield, std::chrono::milliseconds{1});
        EXPECT_TRUE(error.has_value());
    });
}

TEST_F(WsConnectionTests, MultipleConnections)
{
    for (size_t i = 0; i < 2; ++i) {
        asio::spawn(ctx, [&](asio::yield_context yield) {
            auto serverConnection = unwrap(server.acceptConnection(yield));
            auto message = serverConnection.receive(yield);

            ASSERT_TRUE(message.has_value());
            EXPECT_EQ(*message, "hello");
        });

        runSpawn([&](asio::yield_context yield) {
            auto connection = builder.plainConnect(yield);
            ASSERT_TRUE(connection.has_value()) << connection.error().message();

            auto error = connection->operator*().write("hello", yield);
            ASSERT_FALSE(error) << error->message();
        });
    }
}

enum class WsConnectionErrorTestsBundle : int { Read = 1, Write = 2 };

struct WsConnectionErrorTests : WsConnectionTestsBase, testing::WithParamInterface<WsConnectionErrorTestsBundle> {};

INSTANTIATE_TEST_SUITE_P(
    WsConnectionErrorTestsGroup,
    WsConnectionErrorTests,
    testing::Values(WsConnectionErrorTestsBundle::Read, WsConnectionErrorTestsBundle::Write),
    [](auto const& info) {
        switch (info.param) {
            case WsConnectionErrorTestsBundle::Read:
                return "Read";
            case WsConnectionErrorTestsBundle::Write:
                return "Write";
        }
        return "Unknown";
    }
);

TEST_P(WsConnectionErrorTests, ReadWriteError)
{
    asio::spawn(ctx, [&](asio::yield_context yield) {
        auto serverConnection = unwrap(server.acceptConnection(yield));

        auto error = serverConnection.close(yield);
        EXPECT_FALSE(error.has_value()) << *error;
    });

    runSpawn([&](asio::yield_context yield) {
        auto maybeConnection = builder.plainConnect(yield);
        ASSERT_TRUE(maybeConnection.has_value()) << maybeConnection.error().message();
        auto& connection = *maybeConnection;

        auto error = connection->close(yield);
        EXPECT_FALSE(error.has_value()) << error->message();

        switch (GetParam()) {
            case WsConnectionErrorTestsBundle::Read: {
                auto const expected = connection->read(yield);
                EXPECT_FALSE(expected.has_value());
                break;
            }
            case WsConnectionErrorTestsBundle::Write: {
                error = connection->write("hello", yield);
                EXPECT_TRUE(error.has_value());
                break;
            }
        }
    });
}
