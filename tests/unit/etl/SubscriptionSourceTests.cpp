#include "etl/impl/SubscriptionMessageQueue.hpp"
#include "etl/impl/SubscriptionSource.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/Spawn.hpp"
#include "util/TestWsServer.hpp"
#include "util/prometheus/Gauge.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>

using namespace etl::impl;
using testing::MockFunction;
using testing::StrictMock;

struct SubscriptionSourceConnectionTestsBase : SyncAsioContextTest {
    SubscriptionSourceConnectionTestsBase()
    {
        subscriptionSource_.run();
    }

    void
    stopSubscriptionSource()
    {
        util::spawn(ctx_, [this](auto&& yield) { subscriptionSource_.stop(yield); });
    }

    [[maybe_unused]] TestWsConnection
    serverConnection(boost::asio::yield_context yield)
    {
        return serverConnection(wsServer_, yield);
    }

    static TestWsConnection
    serverConnection(TestWsServer& server, boost::asio::yield_context yield)
    {
        // The first one is an SSL attempt
        auto failedConnection = server.acceptConnection(yield);
        [&]() { ASSERT_FALSE(failedConnection); }();

        auto connection = server.acceptConnection(yield);
        [&]() { ASSERT_TRUE(connection) << connection.error().message(); }();

        auto message = connection->receive(yield);
        [&]() {
            ASSERT_TRUE(message);
            EXPECT_EQ(
                message.value(),
                R"JSON({"command":"subscribe","streams":["ledger","manifests","validations","transactions_proposed"]})JSON"
            );
        }();
        return std::move(connection).value();
    }

protected:
    TestWsServer wsServer_{ctx_, "0.0.0.0"};

    StrictMockNetworkValidatedLedgersPtr networkValidatedLedgers_;
    StrictMockSubscriptionManagerSharedPtr subscriptionManager_;
    std::shared_ptr<SubscriptionMessageQueue> messageQueue_ =
        std::make_shared<SubscriptionMessageQueue>(ctx_, subscriptionManager_);

    StrictMock<MockFunction<void()>> onConnectHook_;
    StrictMock<MockFunction<void(bool)>> onDisconnectHook_;
    StrictMock<MockFunction<void()>> onLedgerClosedHook_;

    SubscriptionSource subscriptionSource_{
        ctx_,
        "127.0.0.1",
        wsServer_.port(),
        networkValidatedLedgers_,
        messageQueue_,
        onConnectHook_.AsStdFunction(),
        onDisconnectHook_.AsStdFunction(),
        onLedgerClosedHook_.AsStdFunction(),
        std::chrono::milliseconds(5),
        std::chrono::milliseconds(5)
    };
};

struct SubscriptionSourceConnectionTests : util::prometheus::WithPrometheus,
                                           SubscriptionSourceConnectionTestsBase {};

TEST_F(SubscriptionSourceConnectionTests, ConnectionFailed)
{
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    runContext();
}

TEST_F(SubscriptionSourceConnectionTests, ConnectionFailed_Retry_ConnectionFailed)
{
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() {
        stopSubscriptionSource();
    });
    runContext();
}

TEST_F(SubscriptionSourceConnectionTests, ReadError)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    runContext();
}

TEST_F(SubscriptionSourceConnectionTests, ReadTimeout)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    runContext();
}

TEST_F(SubscriptionSourceConnectionTests, ReadError_Reconnect)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        for (int i = 0; i < 2; ++i) {
            auto connection = serverConnection(yield);
            connection.close(yield);
        }
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() {
        stopSubscriptionSource();
    });
    runContext();
}

TEST_F(SubscriptionSourceConnectionTests, IsConnected)
{
    EXPECT_FALSE(subscriptionSource_.isConnected());
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).WillOnce([this]() {
        EXPECT_TRUE(subscriptionSource_.isConnected());
    });
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() {
        EXPECT_FALSE(subscriptionSource_.isConnected());
        stopSubscriptionSource();
    });
    runContext();
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

struct SubscriptionSourceReadTests : util::prometheus::WithPrometheus,
                                     SubscriptionSourceReadTestsBase {};

TEST_F(SubscriptionSourceReadTests, GotWrongMessage_Reconnect)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage("something", yield);
        // We have to schedule receiving to receive close frame and boost will handle it
        // automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() {
        stopSubscriptionSource();
    });
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotResult)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"JSON({"result":{})JSON", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithLedgerIndex)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection =
            connectAndSendMessage(R"JSON({"result":{"ledger_index":123}})JSON", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(*networkValidatedLedgers_, push(123));
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithLedgerIndexAsString_Reconnect)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection =
            connectAndSendMessage(R"JSON({"result":{"ledger_index":"123"}})JSON", yield);
        // We have to schedule receiving to receive close frame and boost will handle it
        // automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() {
        stopSubscriptionSource();
    });
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithValidatedLedgersAsNumber_Reconnect)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection =
            connectAndSendMessage(R"JSON({"result":{"validated_ledgers":123}})JSON", yield);
        // We have to schedule receiving to receive close frame and boost will handle it
        // automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() {
        stopSubscriptionSource();
    });
    runContext();
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

    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(
            R"JSON({"result":{"validated_ledgers":"123-456,789,32"}})JSON", yield
        );
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    runContext();

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
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(
            R"JSON({"result":{"validated_ledgers":"123-456-789,32"}})JSON", yield
        );
        // We have to schedule receiving to receive close frame and boost will handle it
        // automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() {
        stopSubscriptionSource();
    });
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotResultWithLedgerIndexAndValidatedLedgers)
{
    EXPECT_FALSE(subscriptionSource_.hasLedger(1));
    EXPECT_FALSE(subscriptionSource_.hasLedger(1));
    EXPECT_FALSE(subscriptionSource_.hasLedger(2));
    EXPECT_FALSE(subscriptionSource_.hasLedger(3));
    EXPECT_FALSE(subscriptionSource_.hasLedger(4));

    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(
            R"JSON({"result":{"ledger_index":123,"validated_ledgers":"1-3"}})JSON", yield
        );
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(*networkValidatedLedgers_, push(123));
    runContext();

    EXPECT_EQ(subscriptionSource_.validatedRange(), "1-3");
    EXPECT_FALSE(subscriptionSource_.hasLedger(0));
    EXPECT_TRUE(subscriptionSource_.hasLedger(1));
    EXPECT_TRUE(subscriptionSource_.hasLedger(2));
    EXPECT_TRUE(subscriptionSource_.hasLedger(3));
    EXPECT_FALSE(subscriptionSource_.hasLedger(4));
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosed)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"JSON({"type":"ledgerClosed"})JSON", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosedForwardingIsSet)
{
    subscriptionSource_.setForwarding(true);

    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"JSON({"type": "ledgerClosed"})JSON", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onLedgerClosedHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() {
        EXPECT_FALSE(subscriptionSource_.isForwarding());
        stopSubscriptionSource();
    });
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosedWithLedgerIndex)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection =
            connectAndSendMessage(R"JSON({"type": "ledgerClosed","ledger_index": 123})JSON", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(*networkValidatedLedgers_, push(123));
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosedWithLedgerIndexAsString_Reconnect)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(
            R"JSON({"type":"ledgerClosed","ledger_index":"123"}})JSON", yield
        );
        // We have to schedule receiving to receive close frame and boost will handle it
        // automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() {
        stopSubscriptionSource();
    });
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GorLedgerClosedWithValidatedLedgersAsNumber_Reconnect)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(
            R"JSON({"type":"ledgerClosed","validated_ledgers":123})JSON", yield
        );
        // We have to schedule receiving to receive close frame and boost will handle it
        // automatically
        connection.receive(yield);
        serverConnection(yield);
    });

    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([]() {}).WillOnce([this]() {
        stopSubscriptionSource();
    });
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotLedgerClosedWithValidatedLedgers)
{
    EXPECT_FALSE(subscriptionSource_.hasLedger(0));
    EXPECT_FALSE(subscriptionSource_.hasLedger(1));
    EXPECT_FALSE(subscriptionSource_.hasLedger(2));
    EXPECT_FALSE(subscriptionSource_.hasLedger(3));

    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(
            R"JSON({"type":"ledgerClosed","validated_ledgers":"1-2"})JSON", yield
        );
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    runContext();

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

    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(
            R"JSON({"type":"ledgerClosed","ledger_index":123,"validated_ledgers":"1-2"})JSON", yield
        );
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(*networkValidatedLedgers_, push(123));
    runContext();

    EXPECT_FALSE(subscriptionSource_.hasLedger(0));
    EXPECT_TRUE(subscriptionSource_.hasLedger(1));
    EXPECT_TRUE(subscriptionSource_.hasLedger(2));
    EXPECT_FALSE(subscriptionSource_.hasLedger(3));
    EXPECT_EQ(subscriptionSource_.validatedRange(), "1-2");
}

TEST_F(SubscriptionSourceReadTests, GotTransactionFromBackup)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection =
            connectAndSendMessage(R"JSON({"transaction":"some_transaction_data"})JSON", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(
        *subscriptionManager_,
        forwardProposedTransaction((boost::json::object{{"transaction", "some_transaction_data"}}))
    );
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotTransactionIsForwardingTrue)
{
    subscriptionSource_.setForwarding(true);
    boost::json::object const message = {{"transaction", "some_transaction_data"}};

    util::spawn(ctx_, [&message, this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(boost::json::serialize(message), yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(*subscriptionManager_, forwardProposedTransaction(message));
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotTransactionWithMetaIsForwardingFalse)
{
    subscriptionSource_.setForwarding(true);
    boost::json::object const message = {
        {"transaction", "some_transaction_data"}, {"meta", "some_meta_data"}
    };

    util::spawn(ctx_, [&message, this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(boost::json::serialize(message), yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(*subscriptionManager_, forwardProposedTransaction(message)).Times(0);
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotValidationFromBackup)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"JSON({"type":"validationReceived"})JSON", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(
        *subscriptionManager_,
        forwardValidation((boost::json::object{{"type", "validationReceived"}}))
    );
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotValidationReceivedIsForwardingTrue)
{
    subscriptionSource_.setForwarding(true);
    boost::json::object const message = {{"type", "validationReceived"}};

    util::spawn(ctx_, [&message, this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(boost::json::serialize(message), yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(*subscriptionManager_, forwardValidation(message));
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotManifestFromBackup)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(R"JSON({"type":"manifestReceived"})JSON", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(
        *subscriptionManager_, forwardManifest((boost::json::object{{"type", "manifestReceived"}}))
    );
    runContext();
}

TEST_F(SubscriptionSourceReadTests, GotManifestReceivedIsForwardingTrue)
{
    subscriptionSource_.setForwarding(true);
    boost::json::object const message = {{"type", "manifestReceived"}};

    util::spawn(ctx_, [&message, this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage(boost::json::serialize(message), yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(*subscriptionManager_, forwardManifest(message));
    runContext();
}

TEST_F(SubscriptionSourceReadTests, LastMessageTime)
{
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage("some_message", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    runContext();

    auto const actualLastTimeMessage = subscriptionSource_.lastMessageTime();
    auto const now = std::chrono::steady_clock::now();
    auto const diff =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - actualLastTimeMessage);
    EXPECT_LT(diff, std::chrono::milliseconds(100));
}

struct SubscriptionSourcePrometheusCounterTests : util::prometheus::WithMockPrometheus,
                                                  SubscriptionSourceReadTestsBase {};

TEST_F(SubscriptionSourcePrometheusCounterTests, LastMessageTime)
{
    auto& lastMessageTimeMock = makeMock<util::prometheus::GaugeInt>(
        "subscription_source_last_message_time",
        fmt::format("{{source=\"127.0.0.1:{}\"}}", wsServer_.port())
    );
    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto connection = connectAndSendMessage("some_message", yield);
        connection.close(yield);
    });

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this]() { stopSubscriptionSource(); });
    EXPECT_CALL(lastMessageTimeMock, set).WillOnce([](int64_t value) {
        auto const now = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch()
        )
                             .count();
        EXPECT_LE(now - value, 1);
    });
    runContext();
}

// The ledger marker is an acknowledgement that the preceding stream event was read while
// this source was still a backup. Promotion must not lose that already received event.
struct SubscriptionSourceHandoffTests : util::prometheus::WithPrometheus,
                                        SubscriptionSourceReadTestsBase,
                                        testing::WithParamInterface<std::string> {};

TEST_P(SubscriptionSourceHandoffTests, BackupMessageBeforeHandoffIsNotLost)
{
    auto const stream = GetParam();
    boost::json::object const before = stream == "transaction"
        ? boost::json::object{{"transaction", {{"hash", "BEFORE"}}}}
        : boost::json::object{{"type", stream}, {"signature", "BEFORE"}};
    boost::json::object const after = stream == "transaction"
        ? boost::json::object{{"transaction", {{"hash", "AFTER"}}}}
        : boost::json::object{{"type", stream}, {"signature", "AFTER"}};

    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(*networkValidatedLedgers_, push(123)).WillOnce([this](auto) {
        EXPECT_FALSE(subscriptionSource_.isForwarding());
        subscriptionSource_.setForwarding(true);
    });
    EXPECT_CALL(onDisconnectHook_, Call(true)).WillOnce([this]() { stopSubscriptionSource(); });

    if (stream == "transaction") {
        EXPECT_CALL(*subscriptionManager_, forwardProposedTransaction(before));
        EXPECT_CALL(*subscriptionManager_, forwardProposedTransaction(after));
    } else if (stream == "validationReceived") {
        EXPECT_CALL(*subscriptionManager_, forwardValidation(before));
        EXPECT_CALL(*subscriptionManager_, forwardValidation(after));
    } else {
        EXPECT_CALL(*subscriptionManager_, forwardManifest(before));
        EXPECT_CALL(*subscriptionManager_, forwardManifest(after));
    }

    util::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto connection = serverConnection(yield);
        EXPECT_FALSE(connection.send(boost::json::serialize(before), yield));
        EXPECT_FALSE(connection.send(R"JSON({"result":{"ledger_index":123}})JSON", yield));
        EXPECT_FALSE(connection.send(boost::json::serialize(after), yield));
        connection.close(yield);
    });
    runContext();
}

INSTANTIATE_TEST_SUITE_P(
    Streams,
    SubscriptionSourceHandoffTests,
    testing::Values("transaction", "validationReceived", "manifestReceived")
);

TEST_F(SubscriptionSourceReadTests, TwoSourcesShareDeduplicationAcrossReconnect)
{
    TestWsServer backupServer{ctx_, "0.0.0.0"};
    std::unique_ptr<SubscriptionSource> backup;
    backup = std::make_unique<SubscriptionSource>(
        ctx_,
        "127.0.0.1",
        backupServer.port(),
        networkValidatedLedgers_,
        messageQueue_,
        [] {},
        [&](bool wasForwarding) {
            EXPECT_FALSE(wasForwarding);
            util::spawn(ctx_, [&](auto yield) { backup->stop(yield); });
        },
        [] {},
        std::chrono::seconds{1},
        std::chrono::milliseconds{5}
    );
    subscriptionSource_.setForwarding(true);
    backup->run();

    boost::json::object const duplicated{{"type", "validationReceived"}, {"signature", "SAME"}};
    boost::json::object const unique{{"type", "validationReceived"}, {"signature", "BACKUP_ONLY"}};
    EXPECT_CALL(*subscriptionManager_, forwardValidation(duplicated)).Times(1);
    EXPECT_CALL(*subscriptionManager_, forwardValidation(unique)).Times(1);
    EXPECT_CALL(onConnectHook_, Call()).Times(2);
    EXPECT_CALL(onDisconnectHook_, Call(true)).Times(1);
    EXPECT_CALL(onDisconnectHook_, Call(false)).WillOnce([this] { stopSubscriptionSource(); });

    util::spawn(ctx_, [&](auto yield) {
        // The active source reconnects and receives the same event again. The queue survives
        // that connection lifecycle, and a backup can supply an event the active source missed.
        for (int i = 0; i < 2; ++i) {
            auto connection = serverConnection(yield);
            EXPECT_FALSE(connection.send(boost::json::serialize(duplicated), yield));
            connection.close(yield);
        }
    });
    util::spawn(ctx_, [&](auto yield) {
        auto connection = serverConnection(backupServer, yield);
        EXPECT_FALSE(connection.send(boost::json::serialize(duplicated), yield));
        EXPECT_FALSE(connection.send(boost::json::serialize(unique), yield));
        connection.close(yield);
    });
    runContext();
}

TEST_F(SubscriptionSourceReadTests, StopDrainsAcceptedEvent)
{
    boost::json::object const message{{"transaction", {{"hash", "TX"}}}};
    EXPECT_CALL(onConnectHook_, Call());
    EXPECT_CALL(*subscriptionManager_, forwardProposedTransaction(message))
        .WillOnce([this](auto const&) { stopSubscriptionSource(); });
    util::spawn(ctx_, [&](auto yield) {
        auto connection = connectAndSendMessage(boost::json::serialize(message), yield);
        connection.receive(yield);
    });
    runContext();
    EXPECT_FALSE(subscriptionSource_.isConnected());
}
