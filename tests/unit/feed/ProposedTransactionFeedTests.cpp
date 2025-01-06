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
#include "util/MockPrometheus.hpp"
#include "util/MockWsBase.hpp"
#include "util/SyncExecutionCtxFixture.hpp"
#include "util/TestObject.hpp"
#include "util/prometheus/Gauge.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace {

constexpr auto kACCOUNT1 = "rh1HPuRVsYYvThxG2Bs1MfjmrVC73S16Fb";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kACCOUNT3 = "r92yNeoiCdwULRbjh6cUBEbD71iHcqe1hE";
constexpr auto kDUMMY_TRANSACTION =
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

}  // namespace

using namespace feed::impl;
namespace json = boost::json;
using namespace util::prometheus;

using FeedProposedTransactionTest = FeedBaseTest<ProposedTransactionFeed>;

TEST_F(FeedProposedTransactionTest, ProposedTransaction)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kDUMMY_TRANSACTION)));
    testFeedPtr->pub(json::parse(kDUMMY_TRANSACTION).get_object());

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 0);

    testFeedPtr->pub(json::parse(kDUMMY_TRANSACTION).get_object());
}

TEST_F(FeedProposedTransactionTest, AccountProposedTransaction)
{
    auto const account = getAccountIdWithString(kACCOUNT1);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    web::SubscriptionContextPtr const sessionIdle = std::make_shared<MockSession>();
    auto const accountIdle = getAccountIdWithString(kACCOUNT3);

    EXPECT_CALL(*dynamic_cast<MockSession*>(sessionIdle.get()), onDisconnect);
    testFeedPtr->sub(accountIdle, sessionIdle);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kDUMMY_TRANSACTION)));

    testFeedPtr->pub(json::parse(kDUMMY_TRANSACTION).get_object());

    // unsub
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    testFeedPtr->pub(json::parse(kDUMMY_TRANSACTION).get_object());
}

TEST_F(FeedProposedTransactionTest, SubStreamAndAccount)
{
    auto const account = getAccountIdWithString(kACCOUNT1);

    EXPECT_CALL(*mockSessionPtr, onDisconnect).Times(2);
    testFeedPtr->sub(account, sessionPtr);
    testFeedPtr->sub(sessionPtr);

    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kDUMMY_TRANSACTION))).Times(2);

    testFeedPtr->pub(json::parse(kDUMMY_TRANSACTION).get_object());

    // unsub
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kDUMMY_TRANSACTION)));

    testFeedPtr->pub(json::parse(kDUMMY_TRANSACTION).get_object());

    // unsub transaction
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 0);

    testFeedPtr->pub(json::parse(kDUMMY_TRANSACTION).get_object());
}

TEST_F(FeedProposedTransactionTest, AccountProposedTransactionDuplicate)
{
    auto const account = getAccountIdWithString(kACCOUNT1);
    auto const account2 = getAccountIdWithString(kACCOUNT2);

    EXPECT_CALL(*mockSessionPtr, onDisconnect).Times(2);
    testFeedPtr->sub(account, sessionPtr);
    testFeedPtr->sub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kDUMMY_TRANSACTION)));
    testFeedPtr->pub(json::parse(kDUMMY_TRANSACTION).get_object());

    // unsub account1
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kDUMMY_TRANSACTION)));
    testFeedPtr->pub(json::parse(kDUMMY_TRANSACTION).get_object());

    // unsub account2
    testFeedPtr->unsub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    testFeedPtr->pub(json::parse(kDUMMY_TRANSACTION).get_object());
}

TEST_F(FeedProposedTransactionTest, Count)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);
    // repeat
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    auto const account1 = getAccountIdWithString(kACCOUNT1);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account1, sessionPtr);
    // repeat
    testFeedPtr->sub(account1, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    auto const sessionPtr2 = std::make_shared<MockSession>();

    EXPECT_CALL(*dynamic_cast<MockSession*>(sessionPtr2.get()), onDisconnect);
    testFeedPtr->sub(sessionPtr2);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 2);

    auto const account2 = getAccountIdWithString(kACCOUNT2);

    EXPECT_CALL(*dynamic_cast<MockSession*>(sessionPtr2.get()), onDisconnect);
    testFeedPtr->sub(account2, sessionPtr2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    EXPECT_CALL(*dynamic_cast<MockSession*>(sessionPtr2.get()), onDisconnect);
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
    std::vector<web::SubscriptionContextInterface::OnDisconnectSlot> sessionOnDisconnectSlots;
    ON_CALL(*mockSessionPtr, onDisconnect).WillByDefault([&sessionOnDisconnectSlots](auto slot) {
        sessionOnDisconnectSlots.push_back(slot);
    });
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);
    // repeat
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    auto const account1 = getAccountIdWithString(kACCOUNT1);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account1, sessionPtr);
    // repeat
    testFeedPtr->sub(account1, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    auto sessionPtr2 = std::make_shared<MockSession>();
    auto mockSessionPtr2 = dynamic_cast<MockSession*>(sessionPtr2.get());
    std::vector<web::SubscriptionContextInterface::OnDisconnectSlot> session2OnDisconnectSlots;
    ON_CALL(*mockSessionPtr2, onDisconnect).WillByDefault([&session2OnDisconnectSlots](auto slot) {
        session2OnDisconnectSlots.push_back(slot);
    });

    EXPECT_CALL(*mockSessionPtr2, onDisconnect);
    testFeedPtr->sub(sessionPtr2);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 2);

    auto const account2 = getAccountIdWithString(kACCOUNT2);

    EXPECT_CALL(*mockSessionPtr2, onDisconnect);
    testFeedPtr->sub(account2, sessionPtr2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    EXPECT_CALL(*mockSessionPtr2, onDisconnect);
    testFeedPtr->sub(account1, sessionPtr2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 3);

    std::ranges::for_each(session2OnDisconnectSlots, [&sessionPtr2](auto& slot) { slot(sessionPtr2.get()); });
    sessionPtr2.reset();
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 1);

    std::ranges::for_each(sessionOnDisconnectSlots, [this](auto& slot) { slot(sessionPtr.get()); });
    sessionPtr.reset();
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
    EXPECT_EQ(testFeedPtr->transactionSubcount(), 0);
}

struct ProposedTransactionFeedMockPrometheusTest : WithMockPrometheus, SyncExecutionCtxFixture {
protected:
    web::SubscriptionContextPtr sessionPtr_ = std::make_shared<MockSession>();
    std::shared_ptr<ProposedTransactionFeed> testFeedPtr_ = std::make_shared<ProposedTransactionFeed>(ctx_);
    MockSession* mockSessionPtr_ = dynamic_cast<MockSession*>(sessionPtr_.get());
};

TEST_F(ProposedTransactionFeedMockPrometheusTest, subUnsub)
{
    auto& counterTx = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"tx_proposed\"}");
    auto& counterAccount = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"account_proposed\"}");

    EXPECT_CALL(counterTx, add(1));
    EXPECT_CALL(counterTx, add(-1));
    EXPECT_CALL(counterAccount, add(1));
    EXPECT_CALL(counterAccount, add(-1));

    EXPECT_CALL(*mockSessionPtr_, onDisconnect);
    testFeedPtr_->sub(sessionPtr_);
    testFeedPtr_->unsub(sessionPtr_);

    auto const account = getAccountIdWithString(kACCOUNT1);
    EXPECT_CALL(*mockSessionPtr_, onDisconnect);
    testFeedPtr_->sub(account, sessionPtr_);
    testFeedPtr_->unsub(account, sessionPtr_);
}

TEST_F(ProposedTransactionFeedMockPrometheusTest, AutoDisconnect)
{
    auto& counterTx = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"tx_proposed\"}");
    auto& counterAccount = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"account_proposed\"}");

    std::vector<web::SubscriptionContextInterface::OnDisconnectSlot> sessionOnDisconnectSlots;

    EXPECT_CALL(counterTx, add(1));
    EXPECT_CALL(counterTx, add(-1));
    EXPECT_CALL(counterAccount, add(1));
    EXPECT_CALL(counterAccount, add(-1));

    EXPECT_CALL(*mockSessionPtr_, onDisconnect).WillOnce([&sessionOnDisconnectSlots](auto slot) {
        sessionOnDisconnectSlots.push_back(slot);
    });
    testFeedPtr_->sub(sessionPtr_);

    auto const account = getAccountIdWithString(kACCOUNT1);
    EXPECT_CALL(*mockSessionPtr_, onDisconnect).WillOnce([&sessionOnDisconnectSlots](auto slot) {
        sessionOnDisconnectSlots.push_back(slot);
    });
    testFeedPtr_->sub(account, sessionPtr_);

    std::ranges::for_each(sessionOnDisconnectSlots, [this](auto& slot) { slot(sessionPtr_.get()); });
    sessionPtr_.reset();
}
