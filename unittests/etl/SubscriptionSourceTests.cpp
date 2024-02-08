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

#include "etl/impl/SubscriptionSource.hpp"
#include "gmock/gmock.h"
#include "util/MockNetworkValidatedLedgers.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/TestWsServer.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

using namespace etl::impl;
using testing::MockFunction;
using testing::StrictMock;

struct SubscriptionSourceConnectionTests : public ::testing::Test {
    ~SubscriptionSourceConnectionTests() override
    {
        // SubscriptionSource's destructor posts future on context and waits for it to complete.
        // It is done to be sure that all async operations are completed SubscriptionSource is destroyed.
        // We need a running context to do that.
        std::optional work = boost::asio::make_work_guard(ioContext_);
        std::thread t = std::thread([this]() {
            ioContext_.reset();
            ioContext_.run();
        });
        subscriptionSource_.reset();
        work.reset();
        t.join();
    }

    boost::asio::io_context ioContext_;

    TestWsServer wsServer_{ioContext_, "0.0.0.0", 11113};

    template <typename T>
    using StrictMockPtr = std::shared_ptr<StrictMock<T>>;

    StrictMockPtr<MockNetworkValidatedLedgers> networkValidatedLedgers_ =
        std::make_shared<StrictMock<MockNetworkValidatedLedgers>>();
    StrictMockPtr<MockSubscriptionManager> subscriptionManager_ =
        std::make_shared<StrictMock<MockSubscriptionManager>>();

    StrictMock<MockFunction<void()>> onDisconnectHook_;

    std::unique_ptr<SubscriptionSource> subscriptionSource_ = std::make_unique<SubscriptionSource>(
        ioContext_,
        "127.0.0.1",
        "11113",
        networkValidatedLedgers_,
        subscriptionManager_,
        onDisconnectHook_.AsStdFunction(),
        std::chrono::milliseconds(1),
        std::chrono::milliseconds(1)
    );

    [[maybe_unused]] TestWsConnection
    serverConnection(boost::asio::yield_context yield)
    {
        // The first one is an SSL attempt
        auto failedConnection = wsServer_.acceptConnection(yield);
        [&]() { ASSERT_FALSE(failedConnection); }();

        auto connection = wsServer_.acceptConnection(yield);
        [&]() { ASSERT_TRUE(connection) << connection.error().message(); }();

        auto message = connection->receive(yield);
        [&]() {
            ASSERT_TRUE(message);
            EXPECT_EQ(
                message.value(),
                R"({"command":"subscribe","streams":["ledger","manifests","validations","transactions_proposed"]})"
            );
        }();
        return std::move(connection).value();
    }
};

TEST_F(SubscriptionSourceConnectionTests, ConnectionFailed)
{
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([this]() { subscriptionSource_->stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceConnectionTests, ConnectionFailed_Retry_ConnectionFailed)
{
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_->stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceConnectionTests, ReadError)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        connection.close(yield);
    });
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([this]() { subscriptionSource_->stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceConnectionTests, ReadError_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        for (int i = 0; i < 2; ++i) {
            auto connection = serverConnection(yield);
            connection.close(yield);
        }
    });
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_->stop(); });
    ioContext_.run();
}

struct SubscriptionSourceReadTests : public SubscriptionSourceConnectionTests {
    [[maybe_unused]] TestWsConnection
    connectAndSendMessage(std::string const message, boost::asio::yield_context yield)
    {
        auto connection = serverConnection(yield);
        auto error = connection.send(message, yield);
        [&]() { ASSERT_FALSE(error) << *error; }();
        return connection;
    }
};

TEST_F(SubscriptionSourceReadTests, GotWrongMessage_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage("something", yield);
        serverConnection(yield);
    });
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_->stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResult)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{})", yield);
        connection.close(yield);
    });
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([this]() { subscriptionSource_->stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithLedgerIndex)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"ledger_index":123}})", yield);
        connection.close(yield);
    });
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([this]() { subscriptionSource_->stop(); });
    EXPECT_CALL(*networkValidatedLedgers_, push(123));
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithLedgerIndexAsString_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"ledger_index":"123"}})", yield);
        serverConnection(yield);
    });
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_->stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithValidatedLedgersAsNumber_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"validated_ledgers":123}})", yield);
        serverConnection(yield);
    });
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_->stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithValidatedLedgers)
{
    EXPECT_FALSE(subscriptionSource_->hasLedger(123));
    EXPECT_FALSE(subscriptionSource_->hasLedger(124));
    EXPECT_FALSE(subscriptionSource_->hasLedger(455));
    EXPECT_FALSE(subscriptionSource_->hasLedger(456));
    EXPECT_FALSE(subscriptionSource_->hasLedger(457));
    EXPECT_FALSE(subscriptionSource_->hasLedger(32));
    EXPECT_FALSE(subscriptionSource_->hasLedger(31));
    EXPECT_FALSE(subscriptionSource_->hasLedger(789));
    EXPECT_FALSE(subscriptionSource_->hasLedger(790));

    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"validated_ledgers":"123-456,789,32"}})", yield);
        connection.close(yield);
    });
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([this]() { subscriptionSource_->stop(); });
    ioContext_.run();

    EXPECT_TRUE(subscriptionSource_->hasLedger(123));
    EXPECT_TRUE(subscriptionSource_->hasLedger(124));
    EXPECT_TRUE(subscriptionSource_->hasLedger(455));
    EXPECT_TRUE(subscriptionSource_->hasLedger(456));
    EXPECT_FALSE(subscriptionSource_->hasLedger(457));
    EXPECT_TRUE(subscriptionSource_->hasLedger(32));
    EXPECT_FALSE(subscriptionSource_->hasLedger(31));
    EXPECT_TRUE(subscriptionSource_->hasLedger(789));
    EXPECT_FALSE(subscriptionSource_->hasLedger(790));
}

TEST_F(SubscriptionSourceReadTests, GotResultWithValidatedLedgersWrongValue_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"validated_ledgers":"123-456-789,32"}})", yield);
        serverConnection(yield);
    });
    EXPECT_CALL(onDisconnectHook_, Call()).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_->stop(); });
    ioContext_.run();
}
