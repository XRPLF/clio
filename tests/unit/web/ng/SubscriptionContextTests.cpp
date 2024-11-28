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
#include "util/config/Config.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/SubscriptionContext.hpp"
#include "web/ng/impl/MockWsConnection.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/system/errc.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

using namespace web::ng;

struct ng_SubscriptionContextTests : SyncAsioContextTest {
    util::TagDecoratorFactory tagFactory_{util::Config{}};
    MockWsConnectionImpl connection_{"some ip", boost::beast::flat_buffer{}, tagFactory_};
    testing::StrictMock<testing::MockFunction<bool(Error const&, Connection const&)>> errorHandler_;

    SubscriptionContext
    makeSubscriptionContext(boost::asio::yield_context yield, std::optional<size_t> maxSendQueueSize = std::nullopt)
    {
        return SubscriptionContext{tagFactory_, connection_, maxSendQueueSize, yield, errorHandler_.AsStdFunction()};
    }
};

TEST_F(ng_SubscriptionContextTests, Send)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto subscriptionContext = makeSubscriptionContext(yield);
        auto const message = std::make_shared<std::string>("some message");

        EXPECT_CALL(connection_, sendBuffer).WillOnce([&message](boost::asio::const_buffer buffer, auto, auto) {
            EXPECT_EQ(boost::beast::buffers_to_string(buffer), *message);
            return std::nullopt;
        });
        subscriptionContext.send(message);
        subscriptionContext.disconnect(yield);
    });
}

TEST_F(ng_SubscriptionContextTests, SendOrder)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto subscriptionContext = makeSubscriptionContext(yield);
        auto const message1 = std::make_shared<std::string>("message1");
        auto const message2 = std::make_shared<std::string>("message2");

        testing::Sequence const sequence;
        EXPECT_CALL(connection_, sendBuffer)
            .InSequence(sequence)
            .WillOnce([&message1](boost::asio::const_buffer buffer, auto, auto) {
                EXPECT_EQ(boost::beast::buffers_to_string(buffer), *message1);
                return std::nullopt;
            });
        EXPECT_CALL(connection_, sendBuffer)
            .InSequence(sequence)
            .WillOnce([&message2](boost::asio::const_buffer buffer, auto, auto) {
                EXPECT_EQ(boost::beast::buffers_to_string(buffer), *message2);
                return std::nullopt;
            });

        subscriptionContext.send(message1);
        subscriptionContext.send(message2);
        subscriptionContext.disconnect(yield);
    });
}

TEST_F(ng_SubscriptionContextTests, SendFailed)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto subscriptionContext = makeSubscriptionContext(yield);
        auto const message = std::make_shared<std::string>("some message");

        EXPECT_CALL(connection_, sendBuffer).WillOnce([&message](boost::asio::const_buffer buffer, auto, auto) {
            EXPECT_EQ(boost::beast::buffers_to_string(buffer), *message);
            return boost::system::errc::make_error_code(boost::system::errc::not_supported);
        });
        EXPECT_CALL(errorHandler_, Call).WillOnce(testing::Return(true));
        EXPECT_CALL(connection_, close);
        subscriptionContext.send(message);
        subscriptionContext.disconnect(yield);
    });
}

TEST_F(ng_SubscriptionContextTests, SendTooManySubscriptions)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto subscriptionContext = makeSubscriptionContext(yield, 1);
        auto const message = std::make_shared<std::string>("message1");

        EXPECT_CALL(connection_, sendBuffer)
            .WillOnce([&message](boost::asio::const_buffer buffer, boost::asio::yield_context innerYield, auto) {
                boost::asio::post(innerYield);  // simulate send is slow by switching to another coroutine
                EXPECT_EQ(boost::beast::buffers_to_string(buffer), *message);
                return std::nullopt;
            });
        EXPECT_CALL(connection_, close);

        subscriptionContext.send(message);
        subscriptionContext.send(message);
        subscriptionContext.send(message);
        subscriptionContext.disconnect(yield);
    });
}

TEST_F(ng_SubscriptionContextTests, SendAfterDisconnect)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto subscriptionContext = makeSubscriptionContext(yield);
        auto const message = std::make_shared<std::string>("some message");
        subscriptionContext.disconnect(yield);
        subscriptionContext.send(message);
    });
}

TEST_F(ng_SubscriptionContextTests, OnDisconnect)
{
    testing::StrictMock<testing::MockFunction<void(web::SubscriptionContextInterface*)>> onDisconnect;

    runSpawn([&](boost::asio::yield_context yield) {
        auto subscriptionContext = makeSubscriptionContext(yield);
        subscriptionContext.onDisconnect(onDisconnect.AsStdFunction());
        EXPECT_CALL(onDisconnect, Call(&subscriptionContext));
        subscriptionContext.disconnect(yield);
    });
}

TEST_F(ng_SubscriptionContextTests, SetApiSubversion)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto subscriptionContext = makeSubscriptionContext(yield);
        subscriptionContext.setApiSubversion(42);
        EXPECT_EQ(subscriptionContext.apiSubversion(), 42);
    });
}
