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

#include "etl/impl/ForwardingSource.hpp"
#include "util/Fixtures.hpp"
#include "util/TestWsServer.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>
#include <utility>

using namespace etl::impl;

struct ForwardingSourceTests : SyncAsioContextTest {
    TestWsServer server_{ctx, "0.0.0.0"};
    ForwardingSource forwardingSource{"127.0.0.1", server_.port(), std::chrono::milliseconds{1}};
};

TEST_F(ForwardingSourceTests, ConnectionFailed)
{
    runSpawn([&](boost::asio::yield_context yield) {
        auto result = forwardingSource.forwardToRippled({}, {}, yield);
        EXPECT_FALSE(result);
    });
}

struct ForwardingSourceOperationsTests : ForwardingSourceTests {
    std::string const message_ = R"({"data": "some_data"})";
    boost::json::object const reply_ = {{"reply", "some_reply"}};

    TestWsConnection
    serverConnection(boost::asio::yield_context yield)
    {
        // First connection attempt is SSL handshake so it will fail
        auto failedConnection = server_.acceptConnection(yield);
        [&]() { ASSERT_FALSE(failedConnection); }();

        auto connection = server_.acceptConnection(yield);
        [&]() { ASSERT_TRUE(connection); }();
        return std::move(connection).value();
    }
};

TEST_F(ForwardingSourceOperationsTests, ReadFailed)
{
    boost::asio::spawn(ctx, [&](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        connection.close(yield);
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = forwardingSource.forwardToRippled(boost::json::parse(message_).as_object(), {}, yield);
        EXPECT_FALSE(result);
    });
}

TEST_F(ForwardingSourceOperationsTests, ParseFailed)
{
    boost::asio::spawn(ctx, [&](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);

        auto receivedMessage = connection.receive(yield);
        [&]() { ASSERT_TRUE(receivedMessage); }();
        EXPECT_EQ(boost::json::parse(*receivedMessage), boost::json::parse(message_)) << *receivedMessage;

        auto sendError = connection.send("invalid_json", yield);
        [&]() { ASSERT_FALSE(sendError) << *sendError; }();

        connection.close(yield);
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = forwardingSource.forwardToRippled(boost::json::parse(message_).as_object(), {}, yield);
        EXPECT_FALSE(result);
    });
}

TEST_F(ForwardingSourceOperationsTests, GotNotAnObject)
{
    boost::asio::spawn(ctx, [&](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);

        auto receivedMessage = connection.receive(yield);
        [&]() { ASSERT_TRUE(receivedMessage); }();
        EXPECT_EQ(boost::json::parse(*receivedMessage), boost::json::parse(message_)) << *receivedMessage;

        auto sendError = connection.send(R"(["some_value"])", yield);

        [&]() { ASSERT_FALSE(sendError) << *sendError; }();

        connection.close(yield);
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = forwardingSource.forwardToRippled(boost::json::parse(message_).as_object(), {}, yield);
        EXPECT_FALSE(result);
    });
}

TEST_F(ForwardingSourceOperationsTests, Success)
{
    boost::asio::spawn(ctx, [&](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);

        auto receivedMessage = connection.receive(yield);
        [&]() { ASSERT_TRUE(receivedMessage); }();
        EXPECT_EQ(boost::json::parse(*receivedMessage), boost::json::parse(message_)) << *receivedMessage;

        auto sendError = connection.send(boost::json::serialize(reply_), yield);
        [&]() { ASSERT_FALSE(sendError) << *sendError; }();
    });

    runSpawn([&](boost::asio::yield_context yield) {
        auto result = forwardingSource.forwardToRippled(boost::json::parse(message_).as_object(), "some_ip", yield);
        [&]() { ASSERT_TRUE(result); }();
        auto expectedReply = reply_;
        expectedReply["forwarded"] = true;
        EXPECT_EQ(*result, expectedReply) << *result;
    });
}
