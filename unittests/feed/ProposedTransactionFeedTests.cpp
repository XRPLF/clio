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

#include "feed/FeedBaseTest.h"
#include "feed/impl/ProposedTransactionFeed.h"
#include "util/Fixtures.h"
#include "util/MockPrometheus.h"
#include "util/MockWsBase.h"
#include "util/Taggable.h"
#include "util/TestObject.h"
#include "util/config/Config.h"
#include "util/prometheus/Gauge.h"
#include "web/interface/ConnectionBase.h"

#include <boost/asio/io_context.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

constexpr static auto ACCOUNT1 = "rh1HPuRVsYYvThxG2Bs1MfjmrVC73S16Fb";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto DUMMY_TRANSACTION = R"({
        "transaction":
        {
            "Account":"rh1HPuRVsYYvThxG2Bs1MfjmrVC73S16Fb",
            "Amount":"40000000",
            "Destination":"rDgGprMjMWkJRnJ8M5RXq3SXYD8zuQncPc",
            "Fee":"20",
            "Flags":2147483648,
            "Sequence":13767283,
            "SigningPubKey":"036F3CFFE1EA77C1EEC5DCCA38C83E62E3AC068F8A16369620AF1D609BA5A620B2",
            "TransactionType":"Payment",
            "TxnSignature":"30450221009BD0D563B24E50B26A42F30455AD21C3D5CD4D80174C41F7B54969FFC08DE94C02201FC35320B56D56D1E34D1D281D48AC68CBEDDD6EE9DFA639CCB08BB251453A87",
            "hash":"F44393295DB860C6860769C16F5B23887762F09F87A8D1174E0FCFF9E7247F07"
        }
    })";

using namespace feed::impl;
namespace json = boost::json;
using namespace util::prometheus;

using FeedProposedTransactionTest = FeedBaseTest<ProposedTransactionFeed>;

TEST_F(FeedProposedTransactionTest, ProposedTransaction)
{
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(DUMMY_TRANSACTION));

    cleanReceivedFeed();
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 0);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedProposedTransactionTest, AccountProposedTransaction)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    std::shared_ptr<web::ConnectionBase> const sessionIdle = std::make_shared<MockSession>(tagDecoratorFactory);
    auto const accountIdle = GetAccountIDWithString(ACCOUNT2);
    testFeedPtr->sub(accountIdle, sessionIdle);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(DUMMY_TRANSACTION));

    auto const rawIdle = dynamic_cast<MockSession*>(sessionIdle.get());
    ASSERT_NE(rawIdle, nullptr);
    EXPECT_TRUE(rawIdle->message.empty());

    // unsub
    cleanReceivedFeed();
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedProposedTransactionTest, SubStreamAndAccount)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.run();

    EXPECT_EQ(receivedFeedMessage().size(), json::serialize(json::parse(DUMMY_TRANSACTION)).size() * 2);

    cleanReceivedFeed();
    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();
    EXPECT_EQ(receivedFeedMessage().size(), json::serialize(json::parse(DUMMY_TRANSACTION)).size() * 2);

    // unsub
    cleanReceivedFeed();
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();
    EXPECT_EQ(receivedFeedMessage().size(), json::serialize(json::parse(DUMMY_TRANSACTION)).size());

    // unsub transaction
    cleanReceivedFeed();
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 0);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedProposedTransactionTest, AccountProposedTransactionDuplicate)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    auto const account2 = GetAccountIDWithString(ACCOUNT2);

    testFeedPtr->sub(account, sessionPtr);
    testFeedPtr->sub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    constexpr static auto dummyTransaction = R"({
        "transaction":
        {
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
        }
    })";

    testFeedPtr->pub(json::parse(dummyTransaction).get_object());
    ctx.run();

    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(dummyTransaction));

    // unsub account1
    cleanReceivedFeed();
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    testFeedPtr->pub(json::parse(dummyTransaction).get_object());
    ctx.restart();
    ctx.run();
    EXPECT_EQ(json::parse(receivedFeedMessage()), json::parse(dummyTransaction));

    // unsub account2
    cleanReceivedFeed();
    testFeedPtr->unsub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    testFeedPtr->pub(json::parse(dummyTransaction).get_object());
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(FeedProposedTransactionTest, Count)
{
    testFeedPtr->sub(sessionPtr);
    // repeat
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    auto const account1 = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account1, sessionPtr);
    // repeat
    testFeedPtr->sub(account1, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    auto const sessionPtr2 = std::make_shared<MockSession>(tagDecoratorFactory);
    testFeedPtr->sub(sessionPtr2);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 2);

    auto const account2 = GetAccountIDWithString(ACCOUNT2);
    testFeedPtr->sub(account2, sessionPtr2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);
    testFeedPtr->sub(account1, sessionPtr2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 3);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    // unsub unsubscribed account
    testFeedPtr->unsub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 3);

    testFeedPtr->unsub(account1, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);
    testFeedPtr->unsub(account1, sessionPtr2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    testFeedPtr->unsub(account2, sessionPtr2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
}

TEST_F(FeedProposedTransactionTest, AutoDisconnect)
{
    testFeedPtr->sub(sessionPtr);
    // repeat
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    auto const account1 = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account1, sessionPtr);
    // repeat
    testFeedPtr->sub(account1, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    auto sessionPtr2 = std::make_shared<MockSession>(tagDecoratorFactory);
    testFeedPtr->sub(sessionPtr2);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 2);

    auto const account2 = GetAccountIDWithString(ACCOUNT2);
    testFeedPtr->sub(account2, sessionPtr2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);
    testFeedPtr->sub(account1, sessionPtr2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 3);

    sessionPtr2.reset();
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    sessionPtr.reset();
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 0);
}

struct ProposedTransactionFeedMockPrometheusTest : WithMockPrometheus, SyncAsioContextTest {
protected:
    util::TagDecoratorFactory tagDecoratorFactory{util::Config{}};
    std::shared_ptr<web::ConnectionBase> sessionPtr;
    std::shared_ptr<ProposedTransactionFeed> testFeedPtr;

    void
    SetUp() override
    {
        WithMockPrometheus::SetUp();
        SyncAsioContextTest::SetUp();
        testFeedPtr = std::make_shared<ProposedTransactionFeed>(ctx);
        sessionPtr = std::make_shared<MockSession>(tagDecoratorFactory);
    }
    void
    TearDown() override
    {
        sessionPtr.reset();
        testFeedPtr.reset();
        SyncAsioContextTest::TearDown();
        WithMockPrometheus::TearDown();
    }
};

TEST_F(ProposedTransactionFeedMockPrometheusTest, subUnsub)
{
    auto& counterTx = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"tx_proposed\"}");
    auto& counterAccount = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"account_proposed\"}");

    EXPECT_CALL(counterTx, add(1));
    EXPECT_CALL(counterTx, add(-1));
    EXPECT_CALL(counterAccount, add(1));
    EXPECT_CALL(counterAccount, add(-1));

    testFeedPtr->sub(sessionPtr);
    testFeedPtr->unsub(sessionPtr);

    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr);
    testFeedPtr->unsub(account, sessionPtr);
}

TEST_F(ProposedTransactionFeedMockPrometheusTest, AutoDisconnect)
{
    auto& counterTx = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"tx_proposed\"}");
    auto& counterAccount = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"account_proposed\"}");

    EXPECT_CALL(counterTx, add(1));
    EXPECT_CALL(counterTx, add(-1));
    EXPECT_CALL(counterAccount, add(1));
    EXPECT_CALL(counterAccount, add(-1));

    testFeedPtr->sub(sessionPtr);

    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr);

    sessionPtr.reset();
}
