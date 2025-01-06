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

#include "data/Types.hpp"
#include "feed/FeedTestUtil.hpp"
#include "feed/SubscriptionManager.hpp"
#include "util/Assert.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockWsBase.hpp"
#include "util/TestObject.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/async/context/SyncExecutionContext.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Fees.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/STObject.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace {

constexpr auto kACCOUNT1 = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kCURRENCY = "0158415500000000C1F76FF6ECB0BAC600000000";
constexpr auto kISSUER = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

}  // namespace

namespace json = boost::json;
using namespace feed;
using namespace feed::impl;

template <class Execution>
class SubscriptionManagerBaseTest : public util::prometheus::WithPrometheus, public MockBackendTest {
protected:
    SubscriptionManagerBaseTest()
    {
        ASSERT(sessionPtr_ != nullptr, "dynamic_cast failed");
    }

    std::shared_ptr<SubscriptionManager> subscriptionManagerPtr_ =
        std::make_shared<SubscriptionManager>(Execution(2), backend_);
    web::SubscriptionContextPtr session_ = std::make_shared<MockSession>();
    MockSession* sessionPtr_ = dynamic_cast<MockSession*>(session_.get());
};

using SubscriptionManagerTest = SubscriptionManagerBaseTest<util::async::SyncExecutionContext>;

using SubscriptionManagerAsyncTest = SubscriptionManagerBaseTest<util::async::PoolExecutionContext>;

TEST_F(SubscriptionManagerAsyncTest, MultipleThreadCtx)
{
    EXPECT_CALL(*sessionPtr_, onDisconnect);
    subscriptionManagerPtr_->subManifest(session_);
    EXPECT_CALL(*sessionPtr_, onDisconnect);
    subscriptionManagerPtr_->subValidation(session_);

    static constexpr auto kJSON_MANIFEST = R"({"manifest":"test"})";
    static constexpr auto kJSON_VALIDATION = R"({"validation":"test"})";

    EXPECT_CALL(*sessionPtr_, send(testing::_)).Times(testing::AtMost(2));

    subscriptionManagerPtr_->forwardManifest(json::parse(kJSON_MANIFEST).get_object());
    subscriptionManagerPtr_->forwardValidation(json::parse(kJSON_VALIDATION).get_object());
}

TEST_F(SubscriptionManagerAsyncTest, MultipleThreadCtxSessionDieEarly)
{
    EXPECT_CALL(*sessionPtr_, onDisconnect);
    subscriptionManagerPtr_->subManifest(session_);
    EXPECT_CALL(*sessionPtr_, onDisconnect);
    subscriptionManagerPtr_->subValidation(session_);

    EXPECT_CALL(*sessionPtr_, send(testing::_)).Times(0);
    session_.reset();

    subscriptionManagerPtr_->forwardManifest(json::parse(R"({"manifest":"test"})").get_object());
    subscriptionManagerPtr_->forwardValidation(json::parse(R"({"validation":"test"})").get_object());
}

TEST_F(SubscriptionManagerTest, ReportCurrentSubscriber)
{
    static constexpr auto kREPORT_RETURN =
        R"({
            "ledger":0,
            "transactions":2,
            "transactions_proposed":2,
            "manifests":2,
            "validations":2,
            "account":2,
            "accounts_proposed":2,
            "books":2,
            "book_changes":2
        })";
    web::SubscriptionContextPtr const session1 = std::make_shared<MockSession>();
    MockSession* mockSession1 = dynamic_cast<MockSession*>(session1.get());

    web::SubscriptionContextPtr session2 = std::make_shared<MockSession>();
    MockSession* mockSession2 = dynamic_cast<MockSession*>(session2.get());
    std::vector<web::SubscriptionContextInterface::OnDisconnectSlot> session2OnDisconnectSlots;
    ON_CALL(*mockSession2, onDisconnect).WillByDefault([&session2OnDisconnectSlots](auto slot) {
        session2OnDisconnectSlots.push_back(slot);
    });

    EXPECT_CALL(*mockSession1, onDisconnect).Times(5);
    EXPECT_CALL(*mockSession2, onDisconnect).Times(4);
    subscriptionManagerPtr_->subBookChanges(session1);
    subscriptionManagerPtr_->subBookChanges(session2);
    subscriptionManagerPtr_->subManifest(session1);
    subscriptionManagerPtr_->subManifest(session2);
    subscriptionManagerPtr_->subProposedTransactions(session1);
    subscriptionManagerPtr_->subProposedTransactions(session2);
    subscriptionManagerPtr_->subTransactions(session1);

    // session2->apiSubVersion = 2;
    EXPECT_CALL(*mockSession1, onDisconnect).Times(5);
    EXPECT_CALL(*mockSession2, onDisconnect).Times(6);
    subscriptionManagerPtr_->subTransactions(session2);
    subscriptionManagerPtr_->subValidation(session1);
    subscriptionManagerPtr_->subValidation(session2);
    auto const account = getAccountIdWithString(kACCOUNT1);
    subscriptionManagerPtr_->subAccount(account, session1);
    subscriptionManagerPtr_->subAccount(account, session2);
    subscriptionManagerPtr_->subProposedAccount(account, session1);
    subscriptionManagerPtr_->subProposedAccount(account, session2);
    auto const issue1 = getIssue(kCURRENCY, kISSUER);
    ripple::Book const book{ripple::xrpIssue(), issue1};
    subscriptionManagerPtr_->subBook(book, session1);
    subscriptionManagerPtr_->subBook(book, session2);
    EXPECT_EQ(subscriptionManagerPtr_->report(), json::parse(kREPORT_RETURN));

    // count down when unsub manually
    subscriptionManagerPtr_->unsubBookChanges(session1);
    subscriptionManagerPtr_->unsubManifest(session1);
    subscriptionManagerPtr_->unsubProposedTransactions(session1);
    subscriptionManagerPtr_->unsubTransactions(session1);
    subscriptionManagerPtr_->unsubValidation(session1);
    subscriptionManagerPtr_->unsubAccount(account, session1);
    subscriptionManagerPtr_->unsubProposedAccount(account, session1);
    subscriptionManagerPtr_->unsubBook(book, session1);

    // try to unsub an account which is not subscribed
    auto const account2 = getAccountIdWithString(kACCOUNT2);
    subscriptionManagerPtr_->unsubAccount(account2, session1);
    subscriptionManagerPtr_->unsubProposedAccount(account2, session1);
    auto checkResult = [](json::object reportReturn, int result) {
        EXPECT_EQ(reportReturn["book_changes"], result);
        EXPECT_EQ(reportReturn["validations"], result);
        EXPECT_EQ(reportReturn["transactions_proposed"], result);
        EXPECT_EQ(reportReturn["transactions"], result);
        EXPECT_EQ(reportReturn["manifests"], result);
        EXPECT_EQ(reportReturn["accounts_proposed"], result);
        EXPECT_EQ(reportReturn["account"], result);
        EXPECT_EQ(reportReturn["books"], result);
    };
    checkResult(subscriptionManagerPtr_->report(), 1);

    // count down when session disconnect
    std::ranges::for_each(session2OnDisconnectSlots, [&session2](auto& slot) { slot(session2.get()); });
    session2.reset();
    checkResult(subscriptionManagerPtr_->report(), 0);
}

TEST_F(SubscriptionManagerTest, ManifestTest)
{
    static constexpr auto kDUMMY_MANIFEST = R"({"manifest":"test"})";
    EXPECT_CALL(*sessionPtr_, onDisconnect);
    EXPECT_CALL(*sessionPtr_, send(sharedStringJsonEq(kDUMMY_MANIFEST)));
    subscriptionManagerPtr_->subManifest(session_);
    subscriptionManagerPtr_->forwardManifest(json::parse(kDUMMY_MANIFEST).get_object());

    EXPECT_CALL(*sessionPtr_, send(sharedStringJsonEq(kDUMMY_MANIFEST))).Times(0);
    subscriptionManagerPtr_->unsubManifest(session_);
    subscriptionManagerPtr_->forwardManifest(json::parse(kDUMMY_MANIFEST).get_object());
}

TEST_F(SubscriptionManagerTest, ValidationTest)
{
    static constexpr auto kDUMMY = R"({"validation":"test"})";
    EXPECT_CALL(*sessionPtr_, onDisconnect);
    EXPECT_CALL(*sessionPtr_, send(sharedStringJsonEq(kDUMMY)));
    subscriptionManagerPtr_->subValidation(session_);
    subscriptionManagerPtr_->forwardValidation(json::parse(kDUMMY).get_object());

    EXPECT_CALL(*sessionPtr_, send(sharedStringJsonEq(kDUMMY))).Times(0);
    subscriptionManagerPtr_->unsubValidation(session_);
    subscriptionManagerPtr_->forwardValidation(json::parse(kDUMMY).get_object());
}

TEST_F(SubscriptionManagerTest, BookChangesTest)
{
    EXPECT_CALL(*sessionPtr_, onDisconnect);
    subscriptionManagerPtr_->subBookChanges(session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["book_changes"], 1);

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 32);
    auto transactions = std::vector<TransactionAndMetadata>{};
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = createPaymentTransactionObject(kACCOUNT1, kACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    ripple::STObject const metaObj = createMetaDataForBookChange(kCURRENCY, kISSUER, 22, 1, 3, 3, 1);
    trans1.metadata = metaObj.getSerializer().peekData();
    transactions.push_back(trans1);
    static constexpr auto kBOOK_CHANGE_PUBLISH =
        R"({
            "type":"bookChanges",
            "ledger_index":32,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time":0,
            "changes":
            [
                {
                    "currency_a":"XRP_drops",
                    "currency_b":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD/0158415500000000C1F76FF6ECB0BAC600000000",
                    "volume_a":"2",
                    "volume_b":"2",
                    "high":"-1",
                    "low":"-1",
                    "open":"-1",
                    "close":"-1"
                }
            ]
        })";
    EXPECT_CALL(*sessionPtr_, send(sharedStringJsonEq(kBOOK_CHANGE_PUBLISH)));

    subscriptionManagerPtr_->pubBookChanges(ledgerHeader, transactions);

    subscriptionManagerPtr_->unsubBookChanges(session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["book_changes"], 0);
}

TEST_F(SubscriptionManagerTest, LedgerTest)
{
    backend_->setRange(10, 30);
    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(testing::Return(ledgerHeader));

    auto const feeBlob = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    EXPECT_CALL(*backend_, doFetchLedgerObject).WillOnce(testing::Return(feeBlob));
    // check the function response
    // Information about the ledgers on hand and current fee schedule. This
    // includes the same fields as a ledger stream message, except that it omits
    // the type and txn_count fields
    static constexpr auto kLEDGER_RESPONSE =
        R"({
            "validated_ledgers":"10-30",
            "ledger_index":30,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time":0,
            "fee_base":1,
            "reserve_base":3,
            "reserve_inc":2
        })";
    boost::asio::io_context ctx;
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        EXPECT_CALL(*sessionPtr_, onDisconnect);
        auto const res = subscriptionManagerPtr_->subLedger(yield, session_);
        // check the response
        EXPECT_EQ(res, json::parse(kLEDGER_RESPONSE));
    });
    ctx.run();
    EXPECT_EQ(subscriptionManagerPtr_->report()["ledger"], 1);

    // test publish
    auto const ledgerHeader2 = createLedgerHeader(kLEDGER_HASH, 31);
    auto fee2 = ripple::Fees();
    fee2.reserve = 10;
    static constexpr auto kLEDGER_PUB =
        R"({
            "type":"ledgerClosed",
            "ledger_index":31,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time":0,
            "fee_base":0,
            "reserve_base":10,
            "reserve_inc":0,
            "validated_ledgers":"10-31",
            "txn_count":8
        })";
    EXPECT_CALL(*sessionPtr_, send(sharedStringJsonEq(kLEDGER_PUB)));
    subscriptionManagerPtr_->pubLedger(ledgerHeader2, fee2, "10-31", 8);

    // test unsub
    subscriptionManagerPtr_->unsubLedger(session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["ledger"], 0);
}

TEST_F(SubscriptionManagerTest, TransactionTest)
{
    auto const issue1 = getIssue(kCURRENCY, kISSUER);
    auto const account = getAccountIdWithString(kISSUER);
    ripple::Book const book{ripple::xrpIssue(), issue1};
    EXPECT_CALL(*sessionPtr_, onDisconnect).Times(3);
    subscriptionManagerPtr_->subBook(book, session_);
    subscriptionManagerPtr_->subTransactions(session_);
    subscriptionManagerPtr_->subAccount(account, session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["account"], 1);
    EXPECT_EQ(subscriptionManagerPtr_->report()["transactions"], 1);
    EXPECT_EQ(subscriptionManagerPtr_->report()["books"], 1);

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 33);
    auto trans1 = TransactionAndMetadata();
    auto obj = createPaymentTransactionObject(kACCOUNT1, kACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;

    auto const metaObj = createMetaDataForBookChange(kCURRENCY, kISSUER, 22, 3, 1, 1, 3);
    trans1.metadata = metaObj.getSerializer().peekData();
    static constexpr auto kORDERBOOK_PUBLISH =
        R"({
            "transaction":
            {
                "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Amount":"1",
                "DeliverMax":"1",
                "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Fee":"1",
                "Sequence":32,
                "SigningPubKey":"74657374",
                "TransactionType":"Payment",
                "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "date":0
            },
            "meta":
            {
                "AffectedNodes":
                [
                    {
                        "ModifiedNode":
                        {
                            "FinalFields":
                            {
                                "TakerGets":"3",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value":"1"
                                }
                            },
                            "LedgerEntryType":"Offer",
                            "PreviousFields":
                            {
                                "TakerGets":"1",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value":"3"
                                }
                            }
                        }
                    }
                ],
                "TransactionIndex":22,
                "TransactionResult":"tesSUCCESS",
                "delivered_amount":"unavailable"
            },
            "type":"transaction",
            "validated":true,
            "status":"closed",
            "ledger_index":33,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "engine_result_code":0,
            "engine_result":"tesSUCCESS",
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result_message":"The transaction was applied. Only final in a validated ledger."
        })";
    EXPECT_CALL(*sessionPtr_, send(sharedStringJsonEq(kORDERBOOK_PUBLISH))).Times(3);
    EXPECT_CALL(*sessionPtr_, apiSubversion).Times(3).WillRepeatedly(testing::Return(1));
    subscriptionManagerPtr_->pubTransaction(trans1, ledgerHeader);

    subscriptionManagerPtr_->unsubBook(book, session_);
    subscriptionManagerPtr_->unsubTransactions(session_);
    subscriptionManagerPtr_->unsubAccount(account, session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["account"], 0);
    EXPECT_EQ(subscriptionManagerPtr_->report()["transactions"], 0);
    EXPECT_EQ(subscriptionManagerPtr_->report()["books"], 0);
}

TEST_F(SubscriptionManagerTest, ProposedTransactionTest)
{
    auto const account = getAccountIdWithString(kACCOUNT1);
    EXPECT_CALL(*sessionPtr_, onDisconnect).Times(4);
    subscriptionManagerPtr_->subProposedAccount(account, session_);
    subscriptionManagerPtr_->subProposedTransactions(session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["accounts_proposed"], 1);
    EXPECT_EQ(subscriptionManagerPtr_->report()["transactions_proposed"], 1);

    static constexpr auto kDUMMY_TRANSACTION =
        R"({
            "transaction":
            {
                "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"
            }
        })";
    static constexpr auto kORDERBOOK_PUBLISH =
        R"({
            "transaction":
            {
                "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Amount":"1",
                "DeliverMax":"1",
                "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Fee":"1",
                "Sequence":32,
                "SigningPubKey":"74657374",
                "TransactionType":"Payment",
                "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "date":0
            },
            "meta":
            {
                "AffectedNodes":
                [
                    {
                        "ModifiedNode":
                        {
                            "FinalFields":
                            {
                                "TakerGets":"3",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "value":"1"
                                }
                            },
                            "LedgerEntryType":"Offer",
                            "PreviousFields":
                            {
                                "TakerGets":"1",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "value":"3"
                                }
                            }
                        }
                    }
                ],
                "TransactionIndex":22,
                "TransactionResult":"tesSUCCESS",
                "delivered_amount":"unavailable"
            },
            "type":"transaction",
            "validated":true,
            "status":"closed",
            "ledger_index":33,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "engine_result_code":0,
            "engine_result":"tesSUCCESS",
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result_message":"The transaction was applied. Only final in a validated ledger."
        })";
    EXPECT_CALL(*sessionPtr_, send(sharedStringJsonEq(kDUMMY_TRANSACTION))).Times(2);
    EXPECT_CALL(*sessionPtr_, send(sharedStringJsonEq(kORDERBOOK_PUBLISH))).Times(2);
    subscriptionManagerPtr_->forwardProposedTransaction(json::parse(kDUMMY_TRANSACTION).get_object());

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 33);
    auto trans1 = TransactionAndMetadata();
    auto obj = createPaymentTransactionObject(kACCOUNT1, kACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;

    auto const metaObj = createMetaDataForBookChange(kCURRENCY, kACCOUNT1, 22, 3, 1, 1, 3);
    trans1.metadata = metaObj.getSerializer().peekData();
    EXPECT_CALL(*sessionPtr_, apiSubversion).Times(2).WillRepeatedly(testing::Return(1));
    subscriptionManagerPtr_->pubTransaction(trans1, ledgerHeader);

    // unsub account1
    subscriptionManagerPtr_->unsubProposedAccount(account, session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["accounts_proposed"], 0);
    subscriptionManagerPtr_->unsubProposedTransactions(session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["transactions_proposed"], 0);
}

TEST_F(SubscriptionManagerTest, DuplicateResponseSubTxAndProposedTx)
{
    EXPECT_CALL(*sessionPtr_, onDisconnect).Times(3);
    subscriptionManagerPtr_->subProposedTransactions(session_);
    subscriptionManagerPtr_->subTransactions(session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["transactions"], 1);
    EXPECT_EQ(subscriptionManagerPtr_->report()["transactions_proposed"], 1);

    EXPECT_CALL(*sessionPtr_, send(testing::_)).Times(2);

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 33);
    auto trans1 = TransactionAndMetadata();
    auto obj = createPaymentTransactionObject(kACCOUNT1, kACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;

    auto const metaObj = createMetaDataForBookChange(kCURRENCY, kACCOUNT1, 22, 3, 1, 1, 3);
    trans1.metadata = metaObj.getSerializer().peekData();
    EXPECT_CALL(*sessionPtr_, apiSubversion).Times(2).WillRepeatedly(testing::Return(1));
    subscriptionManagerPtr_->pubTransaction(trans1, ledgerHeader);

    subscriptionManagerPtr_->unsubTransactions(session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["transactions"], 0);
    subscriptionManagerPtr_->unsubProposedTransactions(session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["transactions_proposed"], 0);
}

TEST_F(SubscriptionManagerTest, NoDuplicateResponseSubAccountAndProposedAccount)
{
    auto const account = getAccountIdWithString(kACCOUNT1);
    EXPECT_CALL(*sessionPtr_, onDisconnect).Times(3);
    subscriptionManagerPtr_->subProposedAccount(account, session_);
    subscriptionManagerPtr_->subAccount(account, session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["accounts_proposed"], 1);
    EXPECT_EQ(subscriptionManagerPtr_->report()["account"], 1);

    EXPECT_CALL(*sessionPtr_, send(testing::_));

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 33);
    auto trans1 = TransactionAndMetadata();
    auto obj = createPaymentTransactionObject(kACCOUNT1, kACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;

    auto const metaObj = createMetaDataForBookChange(kCURRENCY, kACCOUNT1, 22, 3, 1, 1, 3);
    trans1.metadata = metaObj.getSerializer().peekData();
    EXPECT_CALL(*sessionPtr_, apiSubversion).WillRepeatedly(testing::Return(1));
    subscriptionManagerPtr_->pubTransaction(trans1, ledgerHeader);

    // unsub account1
    subscriptionManagerPtr_->unsubProposedAccount(account, session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["accounts_proposed"], 0);
    subscriptionManagerPtr_->unsubAccount(account, session_);
    EXPECT_EQ(subscriptionManagerPtr_->report()["account"], 0);
}
