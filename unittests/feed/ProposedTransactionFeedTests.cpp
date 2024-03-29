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

#include "feed/FeedTestUtil.hpp"
#include "feed/impl/ProposedTransactionFeed.hpp"
#include "util/Fixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockWsBase.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

constexpr static auto ACCOUNT1 = "rh1HPuRVsYYvThxG2Bs1MfjmrVC73S16Fb";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto ACCOUNT3 = "r92yNeoiCdwULRbjh6cUBEbD71iHcqe1hE";
constexpr static auto DUMMY_TRANSACTION =
    R"({
        "transaction":
        {
            "Account":"rh1HPuRVsYYvThxG2Bs1MfjmrVC73S16Fb",
            "Amount":"40000000",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
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
using namespace util::prometheus;

using FeedProposedTransactionTest = FeedBaseTest<ProposedTransactionFeed>;

TEST_F(FeedProposedTransactionTest, ProposedTransaction)
{
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(DUMMY_TRANSACTION))).Times(1);
    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.run();

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 0);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();
}

TEST_F(FeedProposedTransactionTest, AccountProposedTransaction)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    std::shared_ptr<web::ConnectionBase> const sessionIdle = std::make_shared<MockSession>();
    auto const accountIdle = GetAccountIDWithString(ACCOUNT3);
    testFeedPtr->sub(accountIdle, sessionIdle);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(DUMMY_TRANSACTION))).Times(1);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.run();

    // unsub
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();
}

TEST_F(FeedProposedTransactionTest, SubStreamAndAccount)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    testFeedPtr->sub(account, sessionPtr);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);
    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(DUMMY_TRANSACTION))).Times(2);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.run();

    // unsub
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(DUMMY_TRANSACTION))).Times(1);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();

    // unsub transaction
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 0);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();
}

TEST_F(FeedProposedTransactionTest, AccountProposedTransactionDuplicate)
{
    auto const account = GetAccountIDWithString(ACCOUNT1);
    auto const account2 = GetAccountIDWithString(ACCOUNT2);

    testFeedPtr->sub(account, sessionPtr);
    testFeedPtr->sub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(DUMMY_TRANSACTION))).Times(1);
    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.run();

    // unsub account1
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(DUMMY_TRANSACTION))).Times(1);
    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();

    // unsub account2
    testFeedPtr->unsub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    testFeedPtr->pub(json::parse(DUMMY_TRANSACTION).get_object());
    ctx.restart();
    ctx.run();
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

    auto const sessionPtr2 = std::make_shared<MockSession>();
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

    auto sessionPtr2 = std::make_shared<MockSession>();
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
    std::shared_ptr<web::ConnectionBase> sessionPtr;
    std::shared_ptr<ProposedTransactionFeed> testFeedPtr;

    void
    SetUp() override
    {
        SyncAsioContextTest::SetUp();
        testFeedPtr = std::make_shared<ProposedTransactionFeed>(ctx);
        sessionPtr = std::make_shared<MockSession>();
    }
    void
    TearDown() override
    {
        sessionPtr.reset();
        testFeedPtr.reset();
        SyncAsioContextTest::TearDown();
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
