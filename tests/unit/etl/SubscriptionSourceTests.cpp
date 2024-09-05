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
#include "util/Fixtures.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/TestWsServer.hpp"
#include "util/prometheus/Gauge.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
#include <utility>

using namespace etl::impl;
using testing::MockFunction;
using testing::StrictMock;

struct SubscriptionSourceConnectionTestsBase : public NoLoggerFixture {
    SubscriptionSourceConnectionTestsBase()
    {
        subscriptionSource_.run();
    }

    boost::asio::io_context ioContext_;
    TestWsServer wsServer_{ioContext_, "0.0.0.0"};

    StrictMockNetworkValidatedLedgersPtr networkValidatedLedgers_;
    StrictMockSubscriptionManagerSharedPtr subscriptionManager_;

    StrictMock<MockFunction<void()>> onConnectHook_;
    StrictMock<MockFunction<void(bool)>> onDisconnectHook_;
    StrictMock<MockFunction<void()>> onLedgerClosedHook_;

    SubscriptionSource subscriptionSource_{
        ioContext_,
        "127.0.0.1",
        wsServer_.port(),
        networkValidatedLedgers_,
        subscriptionManager_,
        onConnectHook_.AsStdFunction(),
        onDisconnectHook_.AsStdFunction(),
        onLedgerClosedHook_.AsStdFunction(),
        std::chrono::milliseconds(5),
        std::chrono::milliseconds(5)
    };

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

struct SubscriptionSourceConnectionTests : util::prometheus::WithPrometheus, SubscriptionSourceConnectionTestsBase {};

TEST_F(SubscriptionSourceConnectionTests, ConnectionFailed)
{
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceConnectionTests, ConnectionFailed_Retry_ConnectionFailed)
{
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceConnectionTests, ReadError)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceConnectionTests, ReadTimeout)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
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

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceConnectionTests, IsConnected)
{
    EXPECT_FALSE(subscriptionSource_.isConnected());
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).WillOnce([this]() { EXPECT_TRUE(subscriptionSource_.isConnected()); });
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() {
        EXPECT_FALSE(subscriptionSource_.isConnected());
        subscriptionSource_.stop();
    });
    ioContext_.run();
}

struct SubscriptionSourceReadTestsBase : public SubscriptionSourceConnectionTestsBase {
    [[maybe_unused]] TestWsConnection
    connectAndSendMessage(std::string const message, boost::asio::yield_context yield)
    {
        auto connection = serverConnection(yield);
        auto error = connection.send(message, yield);
        [&]() { ASSERT_FALSE(error) << *error; }();
        return connection;
    }
};

struct SubscriptionSourceReadTests : util::prometheus::WithPrometheus, SubscriptionSourceReadTestsBase {};

TEST_F(SubscriptionSourceReadTests, GotWrongMessage_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage("something", yield);
        // We have to schedule receiving to receive close frame and boost will handle it automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResult)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithLedgerIndex)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"ledger_index":123}})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    EXPECT_CALL(*networkValidatedLedgers_, push(123));
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithLedgerIndexAsString_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"ledger_index":"123"}})", yield);
        // We have to schedule receiving to receive close frame and boost will handle it automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithValidatedLedgersAsNumber_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"validated_ledgers":123}})", yield);
        // We have to schedule receiving to receive close frame and boost will handle it automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithValidatedLedgers)
{
    EXPECT_FALSE(subscriptionSource_.hasLedger(123));
    EXPECT_FALSE(subscriptionSource_.hasLedger(124));
    EXPECT_FALSE(subscriptionSource_.hasLedger(455));
    EXPECT_FALSE(subscriptionSource_.hasLedger(456));
    EXPECT_FALSE(subscriptionSource_.hasLedger(457));
    EXPECT_FALSE(subscriptionSource_.hasLedger(32));
    EXPECT_FALSE(subscriptionSource_.hasLedger(31));
    EXPECT_FALSE(subscriptionSource_.hasLedger(789));
    EXPECT_FALSE(subscriptionSource_.hasLedger(790));

    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"validated_ledgers":"123-456,789,32"}})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();

    EXPECT_TRUE(subscriptionSource_.hasLedger(123));
    EXPECT_TRUE(subscriptionSource_.hasLedger(124));
    EXPECT_TRUE(subscriptionSource_.hasLedger(455));
    EXPECT_TRUE(subscriptionSource_.hasLedger(456));
    EXPECT_FALSE(subscriptionSource_.hasLedger(457));
    EXPECT_TRUE(subscriptionSource_.hasLedger(32));
    EXPECT_FALSE(subscriptionSource_.hasLedger(31));
    EXPECT_TRUE(subscriptionSource_.hasLedger(789));
    EXPECT_FALSE(subscriptionSource_.hasLedger(790));

    EXPECT_EQ(subscriptionSource_.validatedRange(), "123-456,789,32");
}

TEST_F(SubscriptionSourceReadTests, GotResultWithValidatedLedgersWrongValue_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"validated_ledgers":"123-456-789,32"}})", yield);
        // We have to schedule receiving to receive close frame and boost will handle it automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithLedgerIndexAndValidatedLedgers)
{
    EXPECT_FALSE(subscriptionSource_.hasLedger(1));
    EXPECT_FALSE(subscriptionSource_.hasLedger(1));
    EXPECT_FALSE(subscriptionSource_.hasLedger(2));
    EXPECT_FALSE(subscriptionSource_.hasLedger(3));
    EXPECT_FALSE(subscriptionSource_.hasLedger(4));

    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"result":{"ledger_index":123,"validated_ledgers":"1-3"}})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    EXPECT_CALL(*networkValidatedLedgers_, push(123));
    ioContext_.run();

    EXPECT_EQ(subscriptionSource_.validatedRange(), "1-3");
    EXPECT_FALSE(subscriptionSource_.hasLedger(0));
    EXPECT_TRUE(subscriptionSource_.hasLedger(1));
    EXPECT_TRUE(subscriptionSource_.hasLedger(2));
    EXPECT_TRUE(subscriptionSource_.hasLedger(3));
    EXPECT_FALSE(subscriptionSource_.hasLedger(4));
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosed)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"type":"ledgerClosed"})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosedForwardingIsSet)
{
    subscriptionSource_.setForwarding(true);

    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"type": "ledgerClosed"})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onLedgerClosedHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() {
        EXPECT_FALSE(subscriptionSource_.isForwarding());
        subscriptionSource_.stop();
    });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosedWithLedgerIndex)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"type": "ledgerClosed","ledger_index": 123})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    EXPECT_CALL(*networkValidatedLedgers_, push(123));
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosedWithLedgerIndexAsString_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"type":"ledgerClosed","ledger_index":"123"}})", yield);
        // We have to schedule receiving to receive close frame and boost will handle it automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GorLedgerClosedWithValidatedLedgersAsNumber_Reconnect)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"type":"ledgerClosed","validated_ledgers":123})", yield);
        // We have to schedule receiving to receive close frame and boost will handle it automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosedWithValidatedLedgers)
{
    EXPECT_FALSE(subscriptionSource_.hasLedger(0));
    EXPECT_FALSE(subscriptionSource_.hasLedger(1));
    EXPECT_FALSE(subscriptionSource_.hasLedger(2));
    EXPECT_FALSE(subscriptionSource_.hasLedger(3));

    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"type":"ledgerClosed","validated_ledgers":"1-2"})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();

    EXPECT_FALSE(subscriptionSource_.hasLedger(0));
    EXPECT_TRUE(subscriptionSource_.hasLedger(1));
    EXPECT_TRUE(subscriptionSource_.hasLedger(2));
    EXPECT_FALSE(subscriptionSource_.hasLedger(3));
    EXPECT_EQ(subscriptionSource_.validatedRange(), "1-2");
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosedWithLedgerIndexAndValidatedLedgers)
{
    EXPECT_FALSE(subscriptionSource_.hasLedger(0));
    EXPECT_FALSE(subscriptionSource_.hasLedger(1));
    EXPECT_FALSE(subscriptionSource_.hasLedger(2));
    EXPECT_FALSE(subscriptionSource_.hasLedger(3));

    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection =
            connectAndSendMessage(R"({"type":"ledgerClosed","ledger_index":123,"validated_ledgers":"1-2"})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    EXPECT_CALL(*networkValidatedLedgers_, push(123));
    ioContext_.run();

    EXPECT_FALSE(subscriptionSource_.hasLedger(0));
    EXPECT_TRUE(subscriptionSource_.hasLedger(1));
    EXPECT_TRUE(subscriptionSource_.hasLedger(2));
    EXPECT_FALSE(subscriptionSource_.hasLedger(3));
    EXPECT_EQ(subscriptionSource_.validatedRange(), "1-2");
}

TEST_F(SubscriptionSourceReadTests, GotTransactionIsForwardingFalse)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"transaction":"some_transaction_data"})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotTransactionIsForwardingTrue)
{
    subscriptionSource_.setForwarding(true);
    boost::json::object const message = {{"transaction", "some_transaction_data"}};

    boost::asio::spawn(ioContext_, [&message, this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(boost::json::serialize(message), yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() { subscriptionSource_.stop(); });
    EXPECT_CALL(*subscriptionManager_, forwardProposedTransaction(message));
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotTransactionWithMetaIsForwardingFalse)
{
    subscriptionSource_.setForwarding(true);
    boost::json::object const message = {{"transaction", "some_transaction_data"}, {"meta", "some_meta_data"}};

    boost::asio::spawn(ioContext_, [&message, this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(boost::json::serialize(message), yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() { subscriptionSource_.stop(); });
    EXPECT_CALL(*subscriptionManager_, forwardProposedTransaction(message)).Times(0);
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotValidationReceivedIsForwardingFalse)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"type":"validationReceived"})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotValidationReceivedIsForwardingTrue)
{
    subscriptionSource_.setForwarding(true);
    boost::json::object const message = {{"type", "validationReceived"}};

    boost::asio::spawn(ioContext_, [&message, this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(boost::json::serialize(message), yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() { subscriptionSource_.stop(); });
    EXPECT_CALL(*subscriptionManager_, forwardValidation(message));
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotManiefstReceivedIsForwardingFalse)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"({"type":"manifestReceived"})", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, GotManifestReceivedIsForwardingTrue)
{
    subscriptionSource_.setForwarding(true);
    boost::json::object const message = {{"type", "manifestReceived"}};

    boost::asio::spawn(ioContext_, [&message, this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(boost::json::serialize(message), yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() { subscriptionSource_.stop(); });
    EXPECT_CALL(*subscriptionManager_, forwardManifest(message));
    ioContext_.run();
}

TEST_F(SubscriptionSourceReadTests, LastMessageTime)
{
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage("some_message", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    ioContext_.run();

    auto const actualLastTimeMessage = subscriptionSource_.lastMessageTime();
    auto const now = std::chrono::steady_clock::now();
    auto const diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - actualLastTimeMessage);
    EXPECT_LT(diff, std::chrono::milliseconds(100));
}

struct SubscriptionSourcePrometheusCounterTests : util::prometheus::WithMockPrometheus,
                                                  SubscriptionSourceReadTestsBase {};

TEST_F(SubscriptionSourcePrometheusCounterTests, LastMessageTime)
{
    auto& lastMessageTimeMock = makeMock<util::prometheus::GaugeInt>(
        "subscription_source_last_message_time", fmt::format("{{source=\"127.0.0.1:{}\"}}", wsServer_.port())
    );
    boost::asio::spawn(ioContext_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage("some_message", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { subscriptionSource_.stop(); });
    EXPECT_CALL(lastMessageTimeMock, set).WillOnce([](int64_t value) {
        auto const now =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        EXPECT_LE(std::abs(value - now), 1);
    });
    ioContext_.run();
}
