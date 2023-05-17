//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <subscriptions/SubscriptionManager.h>
#include <util/Fixtures.h>
#include <util/MockBackend.h>
#include <util/MockWsBase.h>
#include <util/TestObject.h>

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>

#include <chrono>

using namespace std::chrono_literals;
namespace json = boost::json;
using namespace Backend;
using ::testing::Return;

// common const
constexpr static auto CURRENCY = "0158415500000000C1F76FF6ECB0BAC600000000";
constexpr static auto ISSUER = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr static auto ACCOUNT1 = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto LEDGERHASH2 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto TXNID = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

/*
 *  test subscription factory method and report function
 */
TEST(SubscriptionManagerTest, InitAndReport)
{
    constexpr static auto ReportReturn = R"({
        "ledger":0,
        "transactions":0,
        "transactions_proposed":0,
        "manifests":0,
        "validations":0,
        "account":0,
        "accounts_proposed":0,
        "books":0,
        "book_changes":0
    })";
    clio::Config cfg;
    auto backend = std::make_shared<MockBackend>(cfg);
    auto subManager = SubscriptionManager::make_SubscriptionManager(cfg, backend);
    EXPECT_EQ(subManager->report(), json::parse(ReportReturn));
}

void
CheckSubscriberMessage(std::string out, std::shared_ptr<Server::ConnectionBase> session, int retry = 10)
{
    auto sessionPtr = static_cast<MockSession*>(session.get());
    while (retry-- != 0)
    {
        std::this_thread::sleep_for(20ms);
        if ((!sessionPtr->message.empty()) && json::parse(sessionPtr->message) == json::parse(out))
        {
            return;
        }
    }
    EXPECT_TRUE(false) << "Could not wait the subscriber message, expect:" << out << " Get:" << sessionPtr->message;
}

// Fixture contains test target and mock backend
class SubscriptionManagerSimpleBackendTest : public MockBackendTest
{
protected:
    clio::Config cfg;
    std::shared_ptr<SubscriptionManager> subManagerPtr;
    util::TagDecoratorFactory tagDecoratorFactory{cfg};
    std::shared_ptr<Server::ConnectionBase> session;
    void
    SetUp() override
    {
        MockBackendTest::SetUp();
        subManagerPtr = SubscriptionManager::make_SubscriptionManager(cfg, mockBackendPtr);
        session = std::make_shared<MockSession>(tagDecoratorFactory);
    }
    void
    TearDown() override
    {
        MockBackendTest::TearDown();
        subManagerPtr.reset();
    }
};

/*
 * test report function and unsub functions
 */
TEST_F(SubscriptionManagerSimpleBackendTest, ReportCurrentSubscriber)
{
    constexpr static auto ReportReturn = R"({
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
    std::shared_ptr<Server::ConnectionBase> session1 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<Server::ConnectionBase> session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    subManagerPtr->subBookChanges(session1);
    subManagerPtr->subBookChanges(session2);
    subManagerPtr->subManifest(session1);
    subManagerPtr->subManifest(session2);
    subManagerPtr->subProposedTransactions(session1);
    subManagerPtr->subProposedTransactions(session2);
    subManagerPtr->subTransactions(session1);
    subManagerPtr->subTransactions(session2);
    subManagerPtr->subValidation(session1);
    subManagerPtr->subValidation(session2);
    auto account = GetAccountIDWithString(ACCOUNT1);
    subManagerPtr->subAccount(account, session1);
    subManagerPtr->subAccount(account, session2);
    subManagerPtr->subProposedAccount(account, session1);
    subManagerPtr->subProposedAccount(account, session2);
    auto issue1 = GetIssue(CURRENCY, ISSUER);
    ripple::Book book{ripple::xrpIssue(), issue1};
    subManagerPtr->subBook(book, session1);
    subManagerPtr->subBook(book, session2);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(subManagerPtr->report(), json::parse(ReportReturn));
    subManagerPtr->unsubBookChanges(session1);
    subManagerPtr->unsubManifest(session1);
    subManagerPtr->unsubProposedTransactions(session1);
    subManagerPtr->unsubTransactions(session1);
    subManagerPtr->unsubValidation(session1);
    subManagerPtr->unsubAccount(account, session1);
    subManagerPtr->unsubProposedAccount(account, session1);
    subManagerPtr->unsubBook(book, session1);
    std::this_thread::sleep_for(20ms);
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
    checkResult(subManagerPtr->report(), 1);
    subManagerPtr->cleanup(session2);
    subManagerPtr->cleanup(session2);  // clean a removed session
    std::this_thread::sleep_for(20ms);
    checkResult(subManagerPtr->report(), 0);
}

TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerLedgerUnSub)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    boost::asio::io_context ctx;
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    // mock fetchLedgerBySequence return this ledger
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // mock doFetchLedgerObject return fee setting ledger object
    auto feeBlob = CreateFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(feeBlob));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) { subManagerPtr->subLedger(yield, session); });
    ctx.run();
    std::this_thread::sleep_for(20ms);
    auto report = subManagerPtr->report();
    EXPECT_EQ(report["ledger"], 1);
    subManagerPtr->cleanup(session);
    subManagerPtr->unsubLedger(session);
    std::this_thread::sleep_for(20ms);
    report = subManagerPtr->report();
    EXPECT_EQ(report["ledger"], 0);
}

/*
 * test Manifest
 * Subscription Manager forward the manifest message to subscribers
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerManifestTest)
{
    subManagerPtr->subManifest(session);
    constexpr static auto dummyManifest = R"({"manifest":"test"})";
    subManagerPtr->forwardManifest(json::parse(dummyManifest).get_object());
    CheckSubscriberMessage(dummyManifest, session);
}

/*
 * test Validation
 * Subscription Manager forward the validation message to subscribers
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerValidation)
{
    subManagerPtr->subValidation(session);
    constexpr static auto dummyValidation = R"({"validation":"test"})";
    subManagerPtr->forwardValidation(json::parse(dummyValidation).get_object());
    CheckSubscriberMessage(dummyValidation, session);
}

/*
 * test ProposedTransaction
 * We don't need the valid transaction in this test, subscription manager just
 * forward the message to subscriber
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerProposedTransaction)
{
    subManagerPtr->subProposedTransactions(session);
    constexpr static auto dummyTransaction = R"({
        "transaction":
        {
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
        }
    })";
    subManagerPtr->forwardProposedTransaction(json::parse(dummyTransaction).get_object());
    CheckSubscriberMessage(dummyTransaction, session);
}

/*
 * test ProposedTransaction for one account
 * we need to construct a valid account in the transaction
 * this test subscribe the proposed transaction for two accounts
 * but only forward a transaction with one of them
 * check the correct session is called
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerAccountProposedTransaction)
{
    auto account = GetAccountIDWithString(ACCOUNT1);
    subManagerPtr->subProposedAccount(account, session);

    std::shared_ptr<Server::ConnectionBase> sessionIdle = std::make_shared<MockSession>(tagDecoratorFactory);
    auto accountIdle = GetAccountIDWithString(ACCOUNT2);
    subManagerPtr->subProposedAccount(accountIdle, sessionIdle);

    constexpr static auto dummyTransaction = R"({
        "transaction":
        {
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
        }
    })";
    subManagerPtr->forwardProposedTransaction(json::parse(dummyTransaction).get_object());
    CheckSubscriberMessage(dummyTransaction, session);
    auto rawIdle = (MockSession*)(sessionIdle.get());
    EXPECT_EQ("", rawIdle->message);
}

/*
 * test ledger stream
 * check 1 subscribe response, 2 publish message
 * mock backend to return fee ledger object
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerLedger)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    boost::asio::io_context ctx;
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 30);
    // mock fetchLedgerBySequence return this ledger
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // mock doFetchLedgerObject return fee setting ledger object
    auto feeBlob = CreateFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(feeBlob));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);
    // check the function response
    // Information about the ledgers on hand and current fee schedule. This
    // includes the same fields as a ledger stream message, except that it omits
    // the type and txn_count fields
    constexpr static auto LedgerResponse = R"({
        "validated_ledgers":"10-30",
        "ledger_index":30,
        "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_time":0,
        "fee_ref":4,
        "fee_base":1,
        "reserve_base":3,
        "reserve_inc":2
    })";
    boost::asio::spawn(ctx, [this](boost::asio::yield_context yield) {
        auto res = subManagerPtr->subLedger(yield, session);
        // check the response
        EXPECT_EQ(res, json::parse(LedgerResponse));
    });
    ctx.run();
    // test publish
    auto ledgerinfo2 = CreateLedgerInfo(LEDGERHASH, 31);
    auto fee2 = ripple::Fees();
    fee2.reserve = 10;
    subManagerPtr->pubLedger(ledgerinfo2, fee2, "10-31", 8);
    constexpr static auto LedgerPub = R"({
        "type":"ledgerClosed",
        "ledger_index":31,
        "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_time":0,
        "fee_ref":0,
        "fee_base":0,
        "reserve_base":10,
        "reserve_inc":0,
        "validated_ledgers":"10-31",
        "txn_count":8
    })";
    CheckSubscriberMessage(LedgerPub, session);
}

/*
 * test book change
 * create a book change meta data for
 * XRP vs A token
 * the transaction is just placeholder
 * Book change computing only needs meta data
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerBookChange)
{
    subManagerPtr->subBookChanges(session);
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH, 32);
    auto transactions = std::vector<TransactionAndMetadata>{};
    auto trans1 = TransactionAndMetadata();
    ripple::STObject obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    ripple::STObject metaObj = CreateMetaDataForBookChange(CURRENCY, ISSUER, 22, 1, 3, 3, 1);
    trans1.metadata = metaObj.getSerializer().peekData();
    transactions.push_back(trans1);
    subManagerPtr->pubBookChanges(ledgerinfo, transactions);
    constexpr static auto BookChangePublish = R"({
        "type":"bookChanges",
        "ledger_index":32,
        "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "ledger_time":0,
        "changes":[
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
    CheckSubscriberMessage(BookChangePublish, session, 20);
}

/*
 * test transaction stream
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerTransaction)
{
    subManagerPtr->subTransactions(session);

    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH2, 33);

    auto trans1 = TransactionAndMetadata();
    ripple::STObject obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    // create an empty meta object
    ripple::STArray metaArray{0};
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();
    subManagerPtr->pubTransaction(trans1, ledgerinfo);
    constexpr static auto TransactionPublish = R"({
        "transaction":{
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount":"1",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Fee":"1",
            "Sequence":32,
            "SigningPubKey":"74657374",
            "TransactionType":"Payment",
            "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
            "date":0
        },
        "meta":{
            "AffectedNodes":[],
            "TransactionIndex":22,
            "TransactionResult":"tesSUCCESS",
            "delivered_amount":"unavailable"
        },
        "type":"transaction",
        "validated":true,
        "status":"closed",
        "ledger_index":33,
        "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
        "engine_result_code":0,
        "engine_result":"tesSUCCESS",
        "engine_result_message":"The transaction was applied. Only final in a validated ledger."
    })";
    CheckSubscriberMessage(TransactionPublish, session);
}

/*
 * test transaction for offer creation
 * check owner_funds
 * mock backend return a trustline
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerTransactionOfferCreation)
{
    subManagerPtr->subTransactions(session);

    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH2, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject obj = CreateCreateOfferTransactionObject(ACCOUNT1, 1, 32, CURRENCY, ISSUER, 1, 3);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    ripple::STArray metaArray{0};
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    ripple::STObject line(ripple::sfIndexes);
    line.setFieldU16(ripple::sfLedgerEntryType, ripple::ltRIPPLE_STATE);
    line.setFieldAmount(ripple::sfLowLimit, ripple::STAmount(10, false));
    line.setFieldAmount(ripple::sfHighLimit, ripple::STAmount(100, false));
    line.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{TXNID});
    line.setFieldU32(ripple::sfPreviousTxnLgrSeq, 3);
    line.setFieldU32(ripple::sfFlags, 0);
    auto issue2 = GetIssue(CURRENCY, ISSUER);
    line.setFieldAmount(ripple::sfBalance, ripple::STAmount(issue2, 100));
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(line.getSerializer().peekData()));
    subManagerPtr->pubTransaction(trans1, ledgerinfo);
    constexpr static auto TransactionForOwnerFund = R"({
        "transaction":{
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Fee":"1",
            "Sequence":32,
            "SigningPubKey":"74657374",
            "TakerGets":{
                "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                "value":"1"
            },
            "TakerPays":"3",
            "TransactionType":"OfferCreate",
            "hash":"EE8775B43A67F4803DECEC5E918E0EA9C56D8ED93E512EBE9F2891846509AAAB",
            "date":0,
            "owner_funds":"100"
        },
        "meta":{
            "AffectedNodes":[],
            "TransactionIndex":22,
            "TransactionResult":"tesSUCCESS"
        },
        "type":"transaction",
        "validated":true,
        "status":"closed",
        "ledger_index":33,
        "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
        "engine_result_code":0,
        "engine_result":"tesSUCCESS",
        "engine_result_message":"The transaction was applied. Only final in a validated ledger."
    })";
    CheckSubscriberMessage(TransactionForOwnerFund, session);
}

constexpr static auto TransactionForOwnerFundFrozen = R"({
    "transaction":{
        "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "Fee":"1",
        "Sequence":32,
        "SigningPubKey":"74657374",
        "TakerGets":{
            "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
            "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
            "value":"1"
        },
        "TakerPays":"3",
        "TransactionType":"OfferCreate",
        "hash":"EE8775B43A67F4803DECEC5E918E0EA9C56D8ED93E512EBE9F2891846509AAAB",
        "date":0,
        "owner_funds":"0"
    },
    "meta":{
        "AffectedNodes":[],
        "TransactionIndex":22,
        "TransactionResult":"tesSUCCESS"
    },
    "type":"transaction",
    "validated":true,
    "status":"closed",
    "ledger_index":33,
    "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
    "engine_result_code":0,
    "engine_result":"tesSUCCESS",
    "engine_result_message":"The transaction was applied. Only final in a validated ledger."
})";

/*
 * test transaction for offer creation
 * check owner_funds when line is frozen
 * mock backend return a trustline
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerTransactionOfferCreationFrozenLine)
{
    subManagerPtr->subTransactions(session);

    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH2, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject obj = CreateCreateOfferTransactionObject(ACCOUNT1, 1, 32, CURRENCY, ISSUER, 1, 3);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    ripple::STArray metaArray{0};
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    ripple::STObject line(ripple::sfIndexes);
    line.setFieldU16(ripple::sfLedgerEntryType, ripple::ltRIPPLE_STATE);
    line.setFieldAmount(ripple::sfLowLimit, ripple::STAmount(10, false));
    line.setFieldAmount(ripple::sfHighLimit, ripple::STAmount(100, false));
    line.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{TXNID});
    line.setFieldU32(ripple::sfPreviousTxnLgrSeq, 3);
    line.setFieldU32(ripple::sfFlags, ripple::lsfHighFreeze);
    line.setFieldAmount(ripple::sfBalance, ripple::STAmount(GetIssue(CURRENCY, ISSUER), 100));
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(line.getSerializer().peekData()));
    subManagerPtr->pubTransaction(trans1, ledgerinfo);
    CheckSubscriberMessage(TransactionForOwnerFundFrozen, session);
}

/*
 * test transaction for offer creation
 * check owner_funds when issue global frozen
 * mock backend return a frozen account setting
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerTransactionOfferCreationGlobalFrozen)
{
    subManagerPtr->subTransactions(session);

    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH2, 33);
    auto trans1 = TransactionAndMetadata();
    ripple::STObject obj = CreateCreateOfferTransactionObject(ACCOUNT1, 1, 32, CURRENCY, ISSUER, 1, 3);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    ripple::STArray metaArray{0};
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    ripple::STObject line(ripple::sfIndexes);
    line.setFieldU16(ripple::sfLedgerEntryType, ripple::ltRIPPLE_STATE);
    line.setFieldAmount(ripple::sfLowLimit, ripple::STAmount(10, false));
    line.setFieldAmount(ripple::sfHighLimit, ripple::STAmount(100, false));
    line.setFieldH256(ripple::sfPreviousTxnID, ripple::uint256{TXNID});
    line.setFieldU32(ripple::sfPreviousTxnLgrSeq, 3);
    line.setFieldU32(ripple::sfFlags, ripple::lsfHighFreeze);
    auto issueAccount = GetAccountIDWithString(ISSUER);
    line.setFieldAmount(ripple::sfBalance, ripple::STAmount(GetIssue(CURRENCY, ISSUER), 100));
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);
    auto kk = ripple::keylet::account(issueAccount).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(testing::_, testing::_, testing::_))
        .WillByDefault(Return(line.getSerializer().peekData()));
    ripple::STObject accountRoot = CreateAccountRootObject(ISSUER, ripple::lsfGlobalFreeze, 1, 10, 2, TXNID, 3);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(kk, testing::_, testing::_))
        .WillByDefault(Return(accountRoot.getSerializer().peekData()));
    subManagerPtr->pubTransaction(trans1, ledgerinfo);
    CheckSubscriberMessage(TransactionForOwnerFundFrozen, session);
}

/*
 * test subscribe account
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerAccount)
{
    auto account = GetAccountIDWithString(ACCOUNT1);
    subManagerPtr->subAccount(account, session);
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH2, 33);

    ripple::STObject obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    auto trans1 = TransactionAndMetadata();
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    ripple::STArray metaArray{1};
    ripple::STObject node(ripple::sfModifiedNode);
    // emplace account into meta, trigger publish
    ripple::STObject finalFields(ripple::sfFinalFields);
    finalFields.setAccountID(ripple::sfAccount, account);
    node.emplace_back(finalFields);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltACCOUNT_ROOT);
    metaArray.push_back(node);
    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldArray(ripple::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    subManagerPtr->pubTransaction(trans1, ledgerinfo);
    constexpr static auto AccountPublish = R"({
        "transaction":{
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount":"1",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Fee":"1",
            "Sequence":32,
            "SigningPubKey":"74657374",
            "TransactionType":"Payment",
            "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
            "date":0
        },
        "meta":{
            "AffectedNodes":[
                {
                    "ModifiedNode":{
                    "FinalFields":{
                        "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                    },
                    "LedgerEntryType":"AccountRoot"
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
        "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
        "engine_result_code":0,
        "engine_result":"tesSUCCESS",
        "engine_result_message":"The transaction was applied. Only final in a validated ledger."
    })";
    CheckSubscriberMessage(AccountPublish, session);
}

/*
 * test subscribe order book
 * Create/Delete/Update offer node will trigger publish
 */
TEST_F(SubscriptionManagerSimpleBackendTest, SubscriptionManagerOrderBook)
{
    auto issue1 = GetIssue(CURRENCY, ISSUER);
    ripple::Book book{ripple::xrpIssue(), issue1};
    subManagerPtr->subBook(book, session);
    auto ledgerinfo = CreateLedgerInfo(LEDGERHASH2, 33);

    auto trans1 = TransactionAndMetadata();
    auto obj = CreatePaymentTransactionObject(ACCOUNT1, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;

    auto metaObj = CreateMetaDataForBookChange(CURRENCY, ISSUER, 22, 3, 1, 1, 3);
    trans1.metadata = metaObj.getSerializer().peekData();
    subManagerPtr->pubTransaction(trans1, ledgerinfo);

    constexpr static auto OrderbookPublish = R"({
        "transaction":{
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount":"1",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Fee":"1",
            "Sequence":32,
            "SigningPubKey":"74657374",
            "TransactionType":"Payment",
            "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
            "date":0
        },
        "meta":{
            "AffectedNodes":[
                {
                    "ModifiedNode":{
                    "FinalFields":{
                        "TakerGets":"3",
                        "TakerPays":{
                            "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                            "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                            "value":"1"
                        }
                    },
                    "LedgerEntryType":"Offer",
                    "PreviousFields":{
                        "TakerGets":"1",
                        "TakerPays":{
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
        "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
        "engine_result_code":0,
        "engine_result":"tesSUCCESS",
        "engine_result_message":"The transaction was applied. Only final in a validated ledger."
    })";
    CheckSubscriberMessage(OrderbookPublish, session);

    // trigger by offer cancel meta data
    std::shared_ptr<Server::ConnectionBase> session1 = std::make_shared<MockSession>(tagDecoratorFactory);
    subManagerPtr->subBook(book, session1);
    metaObj = CreateMetaDataForCancelOffer(CURRENCY, ISSUER, 22, 3, 1);
    trans1.metadata = metaObj.getSerializer().peekData();
    subManagerPtr->pubTransaction(trans1, ledgerinfo);
    constexpr static auto OrderbookCancelPublish = R"({
        "transaction":{
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount":"1",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Fee":"1",
            "Sequence":32,
            "SigningPubKey":"74657374",
            "TransactionType":"Payment",
            "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
            "date":0
        },
        "meta":{
            "AffectedNodes":[
                {
                    "DeletedNode":{
                    "FinalFields":{
                        "TakerGets":"3",
                        "TakerPays":{
                            "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                            "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                            "value":"1"
                        }
                    },
                    "LedgerEntryType":"Offer"
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
        "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
        "engine_result_code":0,
        "engine_result":"tesSUCCESS",
        "engine_result_message":"The transaction was applied. Only final in a validated ledger."
    })";
    CheckSubscriberMessage(OrderbookCancelPublish, session1);
    // trigger by offer create meta data
    constexpr static auto OrderbookCreatePublish = R"({
        "transaction":{
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount":"1",
            "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Fee":"1",
            "Sequence":32,
            "SigningPubKey":"74657374",
            "TransactionType":"Payment",
            "hash":"51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
            "date":0
        },
        "meta":{
            "AffectedNodes":[
                {
                    "CreatedNode":{
                    "NewFields":{
                        "TakerGets":"3",
                        "TakerPays":{
                            "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                            "issuer":"rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                            "value":"1"
                        }
                    },
                    "LedgerEntryType":"Offer"
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
        "ledger_hash":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
        "engine_result_code":0,
        "engine_result":"tesSUCCESS",
        "engine_result_message":"The transaction was applied. Only final in a validated ledger."
    })";
    std::shared_ptr<Server::ConnectionBase> session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    subManagerPtr->subBook(book, session2);
    metaObj = CreateMetaDataForCreateOffer(CURRENCY, ISSUER, 22, 3, 1);
    trans1.metadata = metaObj.getSerializer().peekData();
    subManagerPtr->pubTransaction(trans1, ledgerinfo);
    CheckSubscriberMessage(OrderbookCreatePublish, session2);
}
