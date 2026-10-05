#include "etl/impl/SubscriptionMessageQueue.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/Spawn.hpp"

#include <boost/asio/strand.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

using etl::impl::SubscriptionMessageQueue;
using namespace std::chrono_literals;

struct SubscriptionMessageQueueTests : SyncAsioContextTest {
protected:
    StrictMockSubscriptionManagerSharedPtr subscriptions_;
    SubscriptionMessageQueue::Clock::time_point now_;
    SubscriptionMessageQueue queue_{
        ctx_,
        subscriptions_,
        {.window = 30s, .maxMessages = 2, .maxBytes = 1024},
        [this] { return now_; }
    };
};

TEST_F(SubscriptionMessageQueueTests, DuplicateEventsFromSourcesAreForwardedOnce)
{
    boost::json::object const proposed{{"transaction", {{"hash", "TX"}}}};
    boost::json::object const validation{{"type", "validationReceived"}, {"signature", "VAL"}};
    boost::json::object const manifest{{"type", "manifestReceived"}, {"manifest", "MANIFEST"}};
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(proposed)).Times(1);
    EXPECT_CALL(*subscriptions_, forwardValidation(validation)).Times(1);
    EXPECT_CALL(*subscriptions_, forwardManifest(manifest)).Times(1);
    runSpawn([&](auto yield) {
        for (auto const& message : {proposed, validation, manifest}) {
            queue_.push(message, yield);
            queue_.push(message, yield);
        }
    });
}

TEST_F(SubscriptionMessageQueueTests, ReorderedNestedJsonIsTheSameEvent)
{
    auto const first =
        boost::json::parse(
            R"JSON({"transaction": {"hash": "TX", "Account": "ACCOUNT"}, "status": "proposed"})JSON"
        )
            .as_object();
    auto const reordered =
        boost::json::parse(
            R"JSON({"status": "proposed", "transaction": {"Account": "ACCOUNT", "hash": "TX"}})JSON"
        )
            .as_object();
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(first)).Times(1);
    runSpawn([&](auto yield) {
        queue_.push(first, yield);
        queue_.push(reordered, yield);
    });
}

TEST_F(SubscriptionMessageQueueTests, DifferentValidatorsOnSameLedgerRemainDistinct)
{
    boost::json::object const first{
        {"type", "validationReceived"}, {"ledger_hash", "LEDGER"}, {"signature", "A"}
    };
    auto second = first;
    second["signature"] = "B";
    testing::InSequence const sequence;
    EXPECT_CALL(*subscriptions_, forwardValidation(first));
    EXPECT_CALL(*subscriptions_, forwardValidation(second));
    runSpawn([&](auto yield) {
        queue_.push(first, yield);
        queue_.push(second, yield);
    });
}

TEST_F(SubscriptionMessageQueueTests, DifferentProposalsOfSameTransactionRemainDistinct)
{
    boost::json::object const first{
        {"transaction", {{"hash", "TX"}}}, {"ledger_current_index", 100}
    };
    auto second = first;
    second["ledger_current_index"] = 101;
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(first));
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(second));
    runSpawn([&](auto yield) {
        queue_.push(first, yield);
        queue_.push(second, yield);
    });
}

TEST_F(SubscriptionMessageQueueTests, ExpiredEventCanBeForwardedAgain)
{
    boost::json::object const message{{"transaction", {{"hash", "TX"}}}};
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(message)).Times(2);
    runSpawn([&](auto yield) {
        queue_.push(message, yield);
        now_ += 29s;
        queue_.push(message, yield);
        now_ += 1s;
        queue_.push(message, yield);
    });
}

TEST_F(SubscriptionMessageQueueTests, CacheCapacityEvictsOldestEvent)
{
    boost::json::object const first{{"transaction", {{"hash", "A"}}}};
    boost::json::object const second{{"transaction", {{"hash", "B"}}}};
    boost::json::object const third{{"transaction", {{"hash", "C"}}}};
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(first)).Times(2);
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(second)).Times(1);
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(third)).Times(1);
    runSpawn([&](auto yield) {
        queue_.push(first, yield);
        queue_.push(second, yield);
        queue_.push(third, yield);
        queue_.push(second, yield);
        queue_.push(first, yield);
    });
}

TEST_F(SubscriptionMessageQueueTests, CacheByteLimitEvictsOldestEvent)
{
    SubscriptionMessageQueue queue{
        ctx_, subscriptions_, {.window = 30s, .maxMessages = 100, .maxBytes = 32}
    };
    boost::json::object const first{{"transaction", "A"}};
    boost::json::object const second{{"transaction", "B"}};
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(first)).Times(2);
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(second)).Times(1);
    runSpawn([&](auto yield) {
        queue.push(first, yield);
        queue.push(second, yield);
        queue.push(first, yield);
    });
}

TEST_F(SubscriptionMessageQueueTests, OversizedEventIsForwardedWithoutCaching)
{
    SubscriptionMessageQueue queue{
        ctx_, subscriptions_, {.window = 30s, .maxMessages = 100, .maxBytes = 1}
    };
    boost::json::object const message{{"transaction", "large"}};
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(message)).Times(2);
    runSpawn([&](auto yield) {
        queue.push(message, yield);
        queue.push(message, yield);
    });
}

TEST_F(SubscriptionMessageQueueTests, ValidatedTransactionsAndControlMessagesAreNotForwarded)
{
    runSpawn([&](auto yield) {
        queue_.push({{"transaction", {{"hash", "TX"}}}, {"meta", {}}}, yield);
        queue_.push({{"type", "ledgerClosed"}}, yield);
        queue_.push({{"result", {}}}, yield);
        queue_.push({{"type", "unknown"}}, yield);
    });
}

TEST_F(SubscriptionMessageQueueTests, ForwardingFailureDoesNotPoisonDeduplication)
{
    boost::json::object const message{{"transaction", {{"hash", "TX"}}}};
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(message))
        .WillOnce([](auto const&) { throw std::runtime_error("forwarding failed"); })
        .WillOnce([](auto const&) {});
    runSpawn([&](auto yield) {
        EXPECT_THROW(queue_.push(message, yield), std::runtime_error);
        queue_.push(message, yield);
    });
}

TEST_F(SubscriptionMessageQueueTests, ConcurrentSourcesAreDeduplicated)
{
    boost::json::object const message{{"transaction", {{"hash", "TX"}}}};
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(message)).Times(1);
    for (int i = 0; i < 32; ++i)
        util::spawn(ctx_, [&](auto yield) { queue_.push(message, yield); });
    std::vector<std::thread> runners;
    runners.reserve(4);
    for (int i = 0; i < 4; ++i)
        runners.emplace_back([this] { ctx_.run(); });
    for (auto& runner : runners)
        runner.join();
}

TEST_F(SubscriptionMessageQueueTests, CompletionResumesOnProducerStrand)
{
    auto strand = boost::asio::make_strand(ctx_);
    boost::json::object const message{{"transaction", {{"hash", "TX"}}}};
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(message));
    util::spawn(strand, [&](auto yield) {
        EXPECT_TRUE(strand.running_in_this_thread());
        queue_.push(message, yield);
        EXPECT_TRUE(strand.running_in_this_thread());
    });
    runContext();
}

TEST_F(SubscriptionMessageQueueTests, ZeroCapacityDisablesCaching)
{
    SubscriptionMessageQueue queue{
        ctx_, subscriptions_, {.window = 30s, .maxMessages = 0, .maxBytes = 1024}
    };
    boost::json::object const message{{"transaction", {{"hash", "TX"}}}};
    EXPECT_CALL(*subscriptions_, forwardProposedTransaction(message)).Times(2);
    runSpawn([&](auto yield) {
        queue.push(message, yield);
        queue.push(message, yield);
    });
}
