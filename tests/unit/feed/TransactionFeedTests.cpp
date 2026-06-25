#include "data/AmendmentCenter.hpp"
#include "data/Types.hpp"
#include "feed/FeedTestUtil.hpp"
#include "feed/impl/TransactionFeed.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockWsBase.hpp"
#include "util/SyncExecutionCtxFixture.hpp"
#include "util/TestObject.hpp"
#include "util/prometheus/Gauge.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/TER.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

using namespace data;

namespace {

constexpr auto kAccount1 = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kCurrency = "0158415500000000C1F76FF6ECB0BAC600000000";
constexpr auto kIssuer = "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD";
constexpr auto kTxnId = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kAmmAccount = "rnW8FAPgpQgA6VoESnVrUVJHBdq9QAtRZs";
constexpr auto kLptokenCurrency = "037C35306B24AAB7FF90848206E003279AA47090";
constexpr auto kNetworkId = 0u;
constexpr auto kNftMintId = "000B013A95F14B0044F78A264E41713C64B5F89242540EE208C3098E00000D65";

constexpr auto kTranV1 =
    R"JSON({
        "transaction": {
            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Amount": "1",
            "DeliverMax": "1",
            "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Fee": "1",
            "Sequence": 32,
            "SigningPubKey": "74657374",
            "TransactionType": "Payment",
            "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
            "date": 0
        },
        "meta": {
            "AffectedNodes": [
                {
                    "ModifiedNode": {
                        "FinalFields": {
                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                            "Balance": "110"
                        },
                        "LedgerEntryType": "AccountRoot"
                    }
                },
                {
                    "ModifiedNode": {
                        "FinalFields": {
                            "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                            "Balance": "30"
                        },
                        "LedgerEntryType": "AccountRoot"
                    }
                }
            ],
            "TransactionIndex": 22,
            "TransactionResult": "tesSUCCESS",
            "delivered_amount": "unavailable"
        },
        "ctid": "C000002100160000",
        "type": "transaction",
        "validated": true,
        "status": "closed",
        "ledger_index": 33,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "engine_result_code": 0,
        "engine_result": "tesSUCCESS",
        "engine_result_message": "The transaction was applied. Only final in a validated ledger."
    })JSON";

constexpr auto kTranV2 =
    R"JSON({
        "tx_json": {
            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "DeliverMax": "1",
            "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "Fee": "1",
            "Sequence": 32,
            "SigningPubKey": "74657374",
            "TransactionType": "Payment",
            "date": 0
        },
        "meta": {
            "AffectedNodes": [
                {
                    "ModifiedNode": {
                    "FinalFields": {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Balance": "110"
                    },
                    "LedgerEntryType": "AccountRoot"
                    }
                },
                {
                    "ModifiedNode": {
                    "FinalFields": {
                        "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "Balance": "30"
                    },
                    "LedgerEntryType": "AccountRoot"
                    }
                }
            ],
            "TransactionIndex": 22,
            "TransactionResult": "tesSUCCESS",
            "delivered_amount": "unavailable"
        },
        "ctid": "C000002100160000",
        "type": "transaction",
        "validated": true,
        "status": "closed",
        "ledger_index": 33,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
        "engine_result_code": 0,
        "engine_result": "tesSUCCESS",
        "engine_result_message": "The transaction was applied. Only final in a validated ledger."
    })JSON";

constexpr auto kNftMintTranV1 =
    R"JSON({
        "transaction": {
            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Fee": "12",
            "NFTokenTaxon": 123,
            "Sequence": 1,
            "SigningPubKey": "74657374",
            "TransactionType": "NFTokenMint",
            "hash": "D279CDCA424E5A62A67D98B7E15BCC7AE6E162F19BBAE26E1C7D957109452D0E",
            "date": 0
        },
        "meta": {
            "AffectedNodes": [
                {
                    "ModifiedNode": {
                        "FinalFields": {
                            "NFTokens": [
                                {
                                    "NFToken": {
                                        "NFTokenID": "000B013A95F14B0044F78A264E41713C64B5F89242540EE208C3098E00000D65",
                                        "URI": "7465737475726C"
                                    }
                                },
                                {
                                    "NFToken": {
                                        "NFTokenID": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                                        "URI": "7465737475726C"
                                    }
                                }
                            ]
                        },
                        "LedgerEntryType": "NFTokenPage",
                        "PreviousFields": {
                            "NFTokens": [
                                {
                                    "NFToken": {
                                        "NFTokenID": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                                        "URI": "7465737475726C"
                                    }
                                }
                            ]
                        }
                    }
                }
            ],
            "TransactionIndex": 0,
            "TransactionResult": "tesSUCCESS",
            "nftoken_id": "000B013A95F14B0044F78A264E41713C64B5F89242540EE208C3098E00000D65"
        },
        "ctid": "C000002100000000",
        "type": "transaction",
        "validated": true,
        "status": "closed",
        "ledger_index": 33,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "engine_result_code": 0,
        "engine_result": "tesSUCCESS",
        "engine_result_message": "The transaction was applied. Only final in a validated ledger."
    })JSON";

}  // namespace

using namespace feed::impl;
using namespace util::prometheus;

using FeedTransactionTest = FeedBaseTest<TransactionFeed>;

TEST_F(FeedTransactionTest, SubTransactionV1)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV1)));
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(sessionPtr);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);
}

TEST_F(FeedTransactionTest, SubTransactionForProposedTx)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->subProposed(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV1)));
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsubProposed(sessionPtr);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubTransactionV2)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(2));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV2)));
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubAccountV1)
{
    auto const account = getAccountIdWithString(kAccount1);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);

    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV1)));
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubForProposedAccount)
{
    auto const account = getAccountIdWithString(kAccount1);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->subProposed(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);

    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV1)));
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsubProposed(account, sessionPtr);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubAccountV2)
{
    auto const account = getAccountIdWithString(kAccount1);
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);

    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(2));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV2)));
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubBothTransactionAndAccount)
{
    auto const account = getAccountIdWithString(kAccount1);
    EXPECT_CALL(*mockSessionPtr, onDisconnect).Times(2);
    testFeedPtr->sub(account, sessionPtr);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);

    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).Times(2).WillRepeatedly(testing::Return(2));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV2))).Times(2);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubBookV1)
{
    auto const issue1 = getIssue(kCurrency, kIssuer);
    xrpl::Book const book{xrpl::xrpIssue(), issue1, std::nullopt};

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    auto obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;

    auto metaObj = createMetaDataForBookChange(kCurrency, kIssuer, 22, 3, 1, 1, 3);
    trans1.metadata = metaObj.getSerializer().peekData();

    static constexpr auto kOrderbookPublish =
        R"JSON({
            "transaction": {
                "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Amount": "1",
                "DeliverMax": "1",
                "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Fee": "1",
                "Sequence": 32,
                "SigningPubKey": "74657374",
                "TransactionType": "Payment",
                "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "date": 0
            },
            "meta": {
                "AffectedNodes": [
                    {
                        "ModifiedNode": {
                            "FinalFields": {
                                "TakerGets": "3",
                                "TakerPays": {
                                    "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value": "1"
                                }
                            },
                            "LedgerEntryType": "Offer",
                            "PreviousFields": {
                                "TakerGets": "1",
                                "TakerPays": {
                                    "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value": "3"
                                }
                            }
                        }
                    }
                ],
                "TransactionIndex": 22,
                "TransactionResult": "tesSUCCESS",
                "delivered_amount": "unavailable"
            },
            "ctid": "C000002100160000",
            "type": "transaction",
            "validated": true,
            "status": "closed",
            "ledger_index": 33,
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "engine_result_code": 0,
            "engine_result": "tesSUCCESS",
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result_message": "The transaction was applied. Only final in a validated ledger."
        })JSON";

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kOrderbookPublish))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    // trigger by offer cancel meta data
    metaObj = createMetaDataForCancelOffer(kCurrency, kIssuer, 22, 3, 1);
    trans1.metadata = metaObj.getSerializer().peekData();

    static constexpr auto kOrderbookCancelPublish =
        R"JSON({
            "transaction": {
                "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Amount": "1",
                "DeliverMax": "1",
                "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Fee": "1",
                "Sequence": 32,
                "SigningPubKey": "74657374",
                "TransactionType": "Payment",
                "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "date": 0
            },
            "meta": {
                "AffectedNodes": [
                    {
                        "DeletedNode": {
                            "FinalFields": {
                                "TakerGets": "3",
                                "TakerPays": {
                                    "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value": "1"
                                }
                            },
                            "LedgerEntryType": "Offer"
                        }
                    }
                ],
                "TransactionIndex": 22,
                "TransactionResult": "tesSUCCESS",
                "delivered_amount": "unavailable"
            },
            "ctid": "C000002100160000",
            "type": "transaction",
            "validated": true,
            "status": "closed",
            "ledger_index": 33,
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "engine_result_code": 0,
            "engine_result": "tesSUCCESS",
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result_message": "The transaction was applied. Only final in a validated ledger."
        })JSON";

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kOrderbookCancelPublish))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    // trigger by offer create meta data
    static constexpr auto kOrderbookCreatePublish =
        R"JSON({
            "transaction": {
                "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Amount": "1",
                "DeliverMax": "1",
                "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Fee": "1",
                "Sequence": 32,
                "SigningPubKey": "74657374",
                "TransactionType": "Payment",
                "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "date": 0
            },
            "meta": {
                "AffectedNodes": [
                    {
                        "CreatedNode": {
                            "NewFields": {
                                "TakerGets": "3",
                                "TakerPays": {
                                    "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value": "1"
                                }
                            },
                            "LedgerEntryType": "Offer"
                        }
                    }
                ],
                "TransactionIndex": 22,
                "TransactionResult": "tesSUCCESS",
                "delivered_amount": "unavailable"
            },
            "ctid": "C000002100160000",
            "type": "transaction",
            "validated": true,
            "status": "closed",
            "ledger_index": 33,
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "engine_result_code": 0,
            "engine_result": "tesSUCCESS",
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result_message": "The transaction was applied. Only final in a validated ledger."
        })JSON";
    metaObj = createMetaDataForCreateOffer(kCurrency, kIssuer, 22, 3, 1);
    trans1.metadata = metaObj.getSerializer().peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kOrderbookCreatePublish))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 0);

    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubBookV2)
{
    auto const issue1 = getIssue(kCurrency, kIssuer);
    xrpl::Book const book{xrpl::xrpIssue(), issue1, std::nullopt};

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    auto obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;

    auto const metaObj = createMetaDataForBookChange(kCurrency, kIssuer, 22, 3, 1, 1, 3);
    trans1.metadata = metaObj.getSerializer().peekData();

    static constexpr auto kOrderbookPublish =
        R"JSON({
            "tx_json": {
                "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "DeliverMax": "1",
                "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "Fee": "1",
                "Sequence": 32,
                "SigningPubKey": "74657374",
                "TransactionType": "Payment",
                "date": 0
            },
            "meta": {
                "AffectedNodes": [
                    {
                        "ModifiedNode": {
                            "FinalFields": {
                                "TakerGets": "3",
                                "TakerPays": {
                                    "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value": "1"
                                }
                            },
                            "LedgerEntryType": "Offer",
                            "PreviousFields": {
                                "TakerGets": "1",
                                "TakerPays": {
                                    "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                                    "value": "3"
                                }
                            }
                        }
                    }
                ],
                "TransactionIndex": 22,
                "TransactionResult": "tesSUCCESS",
                "delivered_amount": "unavailable"
            },
            "ctid": "C000002100160000",
            "type": "transaction",
            "validated": true,
            "status": "closed",
            "ledger_index": 33,
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "engine_result_code": 0,
            "engine_result": "tesSUCCESS",
            "close_time_iso": "2000-01-01T00:00:00Z",
            "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
            "engine_result_message": "The transaction was applied. Only final in a validated ledger."
        })JSON";

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(2));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kOrderbookPublish))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 0);

    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, TransactionContainsBothAccountsSubed)
{
    auto const account = getAccountIdWithString(kAccount1);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account, sessionPtr);

    auto const account2 = getAccountIdWithString(kAccount2);
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account2, sessionPtr);

    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(2));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV2))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(2));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV2))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubAccountRepeatWithDifferentVersion)
{
    auto const account = getAccountIdWithString(kAccount1);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account, sessionPtr);

    auto const account2 = getAccountIdWithString(kAccount2);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account2, sessionPtr);

    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(2));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV2))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(2));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV2))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(account2, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubTransactionRepeatWithDifferentVersion)
{
    // sub version 1 first
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);

    // sub version 2 later
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(2));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV2))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubRepeat)
{
    auto const session2 = std::make_shared<MockSession>();

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);

    EXPECT_CALL(*session2, onDisconnect);
    testFeedPtr->sub(session2);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 2);

    testFeedPtr->sub(sessionPtr);
    testFeedPtr->sub(session2);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 2);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);
    testFeedPtr->unsub(session2);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    auto const account = getAccountIdWithString(kAccount1);
    auto const account2 = getAccountIdWithString(kAccount2);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account, sessionPtr);

    EXPECT_CALL(*session2, onDisconnect);
    testFeedPtr->sub(account2, session2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    testFeedPtr->sub(account, sessionPtr);
    testFeedPtr->sub(account2, session2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 2);

    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);
    testFeedPtr->unsub(account2, session2);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);
    testFeedPtr->unsub(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    auto const issue1 = getIssue(kCurrency, kIssuer);
    xrpl::Book const book{xrpl::xrpIssue(), issue1, std::nullopt};

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 1);

    EXPECT_CALL(*session2, onDisconnect);
    testFeedPtr->sub(book, session2);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 2);

    testFeedPtr->unsub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 1);
    testFeedPtr->unsub(book, session2);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 0);
    testFeedPtr->unsub(book, sessionPtr);
    EXPECT_EQ(testFeedPtr->bookSubCount(), 0);
}

TEST_F(FeedTransactionTest, PubTransactionWithOwnerFund)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj =
        createCreateOfferTransactionObject(kAccount1, 1, 32, kCurrency, kIssuer, 1, 3);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    xrpl::STArray const metaArray{0};
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    xrpl::STObject line(xrpl::sfIndexes);
    line.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltRIPPLE_STATE);
    line.setFieldAmount(xrpl::sfLowLimit, xrpl::STAmount(10, false));
    line.setFieldAmount(xrpl::sfHighLimit, xrpl::STAmount(100, false));
    line.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{kTxnId});
    line.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 3);
    line.setFieldU32(xrpl::sfFlags, 0);
    auto const issue2 = getIssue(kCurrency, kIssuer);
    line.setFieldAmount(xrpl::sfBalance, xrpl::STAmount(issue2, 100));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);
    auto const issueAccount = getAccountIdWithString(kIssuer);
    auto const kk = xrpl::keylet::account(issueAccount).key;
    ON_CALL(*backend_, doFetchLedgerObject(testing::_, testing::_, testing::_))
        .WillByDefault(testing::Return(line.getSerializer().peekData()));
    xrpl::STObject const accountRoot = createAccountRootObject(kIssuer, 0, 1, 10, 2, kTxnId, 3);
    ON_CALL(*backend_, doFetchLedgerObject(kk, testing::_, testing::_))
        .WillByDefault(testing::Return(accountRoot.getSerializer().peekData()));

    static constexpr auto kTransactionForOwnerFund =
        R"JSON({
            "transaction": {
                "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Fee": "1",
                "Sequence": 32,
                "SigningPubKey": "74657374",
                "TakerGets": {
                    "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                    "issuer": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                    "value": "1"
                },
                "TakerPays": "3",
                "TransactionType": "OfferCreate",
                "hash": "EE8775B43A67F4803DECEC5E918E0EA9C56D8ED93E512EBE9F2891846509AAAB",
                "date": 0,
                "owner_funds": "100"
            },
            "meta": {
                "AffectedNodes": [],
                "TransactionIndex": 22,
                "TransactionResult": "tesSUCCESS"
            },
            "ctid": "C000002100160000",
            "type": "transaction",
            "validated": true,
            "status": "closed",
            "ledger_index": 33,
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "engine_result_code": 0,
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result": "tesSUCCESS",
            "engine_result_message": "The transaction was applied. Only final in a validated ledger."
        })JSON";

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTransactionForOwnerFund))).Times(1);
    EXPECT_CALL(
        *mockAmendmentCenterPtr_,
        isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_)
    );
    ON_CALL(
        *mockAmendmentCenterPtr_,
        isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_)
    )
        .WillByDefault(testing::Return(false));
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, PublishesNFTokenMintTx)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);

    // Creates an NFTokenMint transaction
    auto const trans = createMintNftTxWithMetadata(kAccount1, 1, 12, 123, kNftMintId);

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kNftMintTranV1)));

    testFeedPtr->pub(trans, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    testFeedPtr->pub(trans, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

static constexpr auto kTranFrozen =
    R"JSON({
        "transaction": {
            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Fee": "1",
            "Sequence": 32,
            "SigningPubKey": "74657374",
            "TakerGets": {
                "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                "issuer": "rK9DrarGKnVEo2nYp5MfVRXRYf5yRX3mwD",
                "value": "1"
            },
            "TakerPays": "3",
            "TransactionType": "OfferCreate",
            "hash": "EE8775B43A67F4803DECEC5E918E0EA9C56D8ED93E512EBE9F2891846509AAAB",
            "date": 0,
            "owner_funds": "0"
        },
        "meta": {
            "AffectedNodes": [],
            "TransactionIndex": 22,
            "TransactionResult": "tesSUCCESS"
        },
        "ctid": "C000002100160000",
        "type": "transaction",
        "validated": true,
        "status": "closed",
        "ledger_index": 33,
        "close_time_iso": "2000-01-01T00:00:00Z",
        "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
        "engine_result_code": 0,
        "engine_result": "tesSUCCESS",
        "engine_result_message": "The transaction was applied. Only final in a validated ledger."
    })JSON";

TEST_F(FeedTransactionTest, PubTransactionOfferCreationFrozenLine)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj =
        createCreateOfferTransactionObject(kAccount1, 1, 32, kCurrency, kIssuer, 1, 3);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    xrpl::STArray const metaArray{0};
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    xrpl::STObject line(xrpl::sfIndexes);
    line.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltRIPPLE_STATE);
    line.setFieldAmount(xrpl::sfLowLimit, xrpl::STAmount(10, false));
    line.setFieldAmount(xrpl::sfHighLimit, xrpl::STAmount(100, false));
    line.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{kTxnId});
    line.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 3);
    line.setFieldU32(xrpl::sfFlags, xrpl::lsfHighFreeze);
    line.setFieldAmount(xrpl::sfBalance, xrpl::STAmount(getIssue(kCurrency, kIssuer), 100));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(3);
    auto const issueAccount = getAccountIdWithString(kIssuer);
    auto const kk = xrpl::keylet::account(issueAccount).key;
    ON_CALL(*backend_, doFetchLedgerObject(testing::_, testing::_, testing::_))
        .WillByDefault(testing::Return(line.getSerializer().peekData()));
    xrpl::STObject const accountRoot = createAccountRootObject(kIssuer, 0, 1, 10, 2, kTxnId, 3);
    ON_CALL(*backend_, doFetchLedgerObject(kk, testing::_, testing::_))
        .WillByDefault(testing::Return(accountRoot.getSerializer().peekData()));

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranFrozen))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubTransactionOfferCreationGlobalFrozen)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj =
        createCreateOfferTransactionObject(kAccount1, 1, 32, kCurrency, kIssuer, 1, 3);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    xrpl::STArray const metaArray{0};
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    xrpl::STObject line(xrpl::sfIndexes);
    line.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltRIPPLE_STATE);
    line.setFieldAmount(xrpl::sfLowLimit, xrpl::STAmount(10, false));
    line.setFieldAmount(xrpl::sfHighLimit, xrpl::STAmount(100, false));
    line.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{kTxnId});
    line.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 3);
    line.setFieldU32(xrpl::sfFlags, xrpl::lsfHighFreeze);
    auto const issueAccount = getAccountIdWithString(kIssuer);
    line.setFieldAmount(xrpl::sfBalance, xrpl::STAmount(getIssue(kCurrency, kIssuer), 100));

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);
    auto const kk = xrpl::keylet::account(issueAccount).key;
    ON_CALL(*backend_, doFetchLedgerObject(testing::_, testing::_, testing::_))
        .WillByDefault(testing::Return(line.getSerializer().peekData()));
    xrpl::STObject const accountRoot =
        createAccountRootObject(kIssuer, xrpl::lsfGlobalFreeze, 1, 10, 2, kTxnId, 3);
    ON_CALL(*backend_, doFetchLedgerObject(kk, testing::_, testing::_))
        .WillByDefault(testing::Return(accountRoot.getSerializer().peekData()));

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranFrozen))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubBothProposedAndValidatedAccount)
{
    auto const account = getAccountIdWithString(kAccount1);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(account, sessionPtr);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->subProposed(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV1))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(account, sessionPtr);
    testFeedPtr->unsubProposed(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubBothProposedAndValidated)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->subProposed(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).Times(2).WillRepeatedly(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV1))).Times(2);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(sessionPtr);
    testFeedPtr->unsubProposed(sessionPtr);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubProposedDisconnect)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->subProposed(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV1))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    sessionPtr.reset();
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, SubProposedAccountDisconnect)
{
    auto const account = getAccountIdWithString(kAccount1);

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->subProposed(account, sessionPtr);
    EXPECT_EQ(testFeedPtr->accountSubCount(), 0);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj = createPaymentTransactionObject(kAccount1, kAccount2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    trans1.metadata = createPaymentTransactionMetaObject(kAccount1, kAccount2, 110, 30, 22)
                          .getSerializer()
                          .peekData();

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTranV1))).Times(1);
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    sessionPtr.reset();
    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

// This test exercises `accountHold` for amendment fixFrozenLPTokenTransfer, so that the output
// shows "owner_funds: 0" if the currency in the amm pool is frozen
TEST_F(FeedTransactionTest, PubTransactionWithOwnerFundFrozenLPToken)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto trans1 = TransactionAndMetadata();
    xrpl::STObject const obj =
        createCreateOfferTransactionObject(kAccount1, 1, 32, kLptokenCurrency, kAmmAccount, 1, 3);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = 32;
    xrpl::STArray const metaArray{0};
    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, metaArray);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, 22);
    trans1.metadata = metaObj.getSerializer().peekData();

    xrpl::STObject line(xrpl::sfIndexes);
    line.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltRIPPLE_STATE);
    line.setFieldAmount(xrpl::sfLowLimit, xrpl::STAmount(10, false));
    line.setFieldAmount(xrpl::sfHighLimit, xrpl::STAmount(100, false));
    line.setFieldH256(xrpl::sfPreviousTxnID, xrpl::uint256{kTxnId});
    line.setFieldU32(xrpl::sfPreviousTxnLgrSeq, 3);
    line.setFieldU32(xrpl::sfFlags, 0);
    auto const issue2 = getIssue(kLptokenCurrency, kAmmAccount);
    line.setFieldAmount(xrpl::sfBalance, xrpl::STAmount(issue2, 100));

    EXPECT_CALL(*backend_, doFetchLedgerObject(testing::_, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(line.getSerializer().peekData()));

    auto const ammID = xrpl::uint256{54321};

    // create an amm account because in `accountHolds` checks for the ammID
    auto const ammAccount = getAccountIdWithString(kAmmAccount);
    auto const kk = xrpl::keylet::account(ammAccount).key;
    xrpl::STObject const ammAccountRoot =
        createAccountRootObject(kAmmAccount, 0, 1, 10, 2, kTxnId, 3, 0, ammID);
    EXPECT_CALL(*backend_, doFetchLedgerObject(kk, testing::_, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(ammAccountRoot.getSerializer().peekData()));

    static constexpr auto kTransactionForOwnerFund =
        R"JSON({
            "transaction": {
                "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Fee": "1",
                "Sequence": 32,
                "SigningPubKey": "74657374",
                "TakerGets": {
                    "currency": "037C35306B24AAB7FF90848206E003279AA47090",
                    "issuer": "rnW8FAPgpQgA6VoESnVrUVJHBdq9QAtRZs",
                    "value": "1"
                },
                "TakerPays": "3",
                "TransactionType": "OfferCreate",
                "hash": "9CA8BBF209DC4505F593A1EA0DC2135A5FA2C6541AF19D128B046873E0CEB695",
                "date": 0,
                "owner_funds": "0"
            },
            "meta": {
                "AffectedNodes": [],
                "TransactionIndex": 22,
                "TransactionResult": "tesSUCCESS"
            },
            "ctid": "C000002100160000",
            "type": "transaction",
            "validated": true,
            "status": "closed",
            "ledger_index": 33,
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "engine_result_code": 0,
            "close_time_iso": "2000-01-01T00:00:00Z",
            "engine_result": "tesSUCCESS",
            "engine_result_message": "The transaction was applied. Only final in a validated ledger."
        })JSON";

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kTransactionForOwnerFund))).Times(1);

    EXPECT_CALL(
        *mockAmendmentCenterPtr_,
        isEnabled(testing::_, Amendments::fixFrozenLPTokenTransfer, testing::_)
    )
        .WillOnce(testing::Return(true));

    auto const ammObj =
        createAmmObject(kAmmAccount, "XRP", xrpl::toBase58(xrpl::xrpAccount()), kCurrency, kIssuer);
    EXPECT_CALL(
        *backend_, doFetchLedgerObject(xrpl::keylet::amm(ammID).key, testing::_, testing::_)
    )
        .WillOnce(testing::Return(ammObj.getSerializer().peekData()));

    // create the issuer account that enacted global freeze
    auto const issuerAccount = getAccountIdWithString(kIssuer);
    xrpl::STObject const issuerAccountRoot =
        createAccountRootObject(kIssuer, 4194304, 1, 10, 2, kTxnId, 3);
    EXPECT_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(issuerAccount).key, testing::_, testing::_)
    )
        .WillOnce(testing::Return(issuerAccountRoot.getSerializer().peekData()));

    testFeedPtr->pub(trans1, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, PublishesMPTokenIssuanceCreateTx)
{
    constexpr auto kMptokenIssuanceCreateTranV1 =
        R"JSON({
            "transaction": {
                "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Fee": "12",
                "Sequence": 1,
                "SigningPubKey": "74657374",
                "TransactionType": "MPTokenIssuanceCreate",
                "hash": "B565E9E541E9C4615C920807AC8104D26F961424A06F3BB25A083DD47680EF45",
                "date": 0
            },
            "meta": {
                "AffectedNodes": [
                    {
                        "CreatedNode": {
                            "LedgerEntryType": "MPTokenIssuance",
                            "LedgerIndex": "0000000000000000000000000000000000000000000000000000000000000000",
                            "NewFields": {
                                "Flags": 0,
                                "Issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "LedgerEntryType": "MPTokenIssuance",
                                "MPTokenMetadata": "746573742D6D657461",
                                "MaximumAmount": "0",
                                "OutstandingAmount": "0",
                                "OwnerNode": "0",
                                "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
                                "PreviousTxnLgrSeq": 0,
                                "Sequence": 1
                            }
                        }
                    }
                ],
                "TransactionIndex": 0,
                "TransactionResult": "tesSUCCESS",
                "mpt_issuance_id": "000000014B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
            },
            "ctid": "C000002100000000",
            "type": "transaction",
            "validated": true,
            "status": "closed",
            "ledger_index": 33,
            "close_time_iso": "2000-01-01T00:00:00Z",
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "engine_result_code": 0,
            "engine_result": "tesSUCCESS",
            "engine_result_message": "The transaction was applied. Only final in a validated ledger."
        })JSON";

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 1);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    auto const trans = createMPTIssuanceCreateTxWithMetadata(kAccount1, 12, 1);

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kMptokenIssuanceCreateTranV1)));

    testFeedPtr->pub(trans, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->transactionSubCount(), 0);

    testFeedPtr->pub(trans, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

TEST_F(FeedTransactionTest, PublishesMPTokenAuthorizeTx)
{
    constexpr auto kMptokenAuthorizeTranV1 =
        R"JSON({
            "transaction": {
                "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Fee": "15",
                "MPTokenIssuanceID": "000000014B4E9C06F24296074F7BC48F92A97916C6DC5EA9",
                "Sequence": 5,
                "SigningPubKey": "74657374",
                "TransactionType": "MPTokenAuthorize",
                "hash": "94ACAB5D571C4A2B8D76979B76E8A82FA91915AEB3FD0A9917223308D5EAE331",
                "date": 0
            },
            "meta": {
                "AffectedNodes": [
                    {
                        "ModifiedNode": {
                            "FinalFields": {
                                "LedgerEntryType": "MPToken",
                                "MPTAmount": "0",
                                "MPTokenIssuanceID": "000000014B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
                            },
                            "LedgerEntryType": "MPToken",
                            "LedgerIndex": "0000000000000000000000000000000000000000000000000000000000000000"
                        }
                    }
                ],
                "TransactionIndex": 0,
                "TransactionResult": "tesSUCCESS"
            },
            "ctid": "C000002100000000",
            "type": "transaction",
            "validated": true,
            "status": "closed",
            "ledger_index": 33,
            "close_time_iso": "2000-01-01T00:00:00Z",
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "engine_result_code": 0,
            "engine_result": "tesSUCCESS",
            "engine_result_message": "The transaction was applied. Only final in a validated ledger."
        })JSON";

    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);

    auto const ledgerHeader = createLedgerHeader(kLedgerHash, 33);
    // The issuance ID that this transaction is authorizing
    auto const mptIssuanceID = xrpl::makeMptID(1, getAccountIdWithString(kAccount1));
    auto const trans = createMPTokenAuthorizeTxWithMetadata(kAccount1, mptIssuanceID, 15, 5);

    EXPECT_CALL(*mockSessionPtr, apiSubversion).WillOnce(testing::Return(1));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kMptokenAuthorizeTranV1)));

    testFeedPtr->pub(trans, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);

    testFeedPtr->unsub(sessionPtr);
    testFeedPtr->pub(trans, ledgerHeader, backend_, mockAmendmentCenterPtr_, kNetworkId);
}

struct TransactionFeedMockPrometheusTest : WithMockPrometheus, SyncExecutionCtxFixture {
protected:
    web::SubscriptionContextPtr sessionPtr_ = std::make_shared<MockSession>();
    std::shared_ptr<TransactionFeed> testFeedPtr_ = std::make_shared<TransactionFeed>(ctx_);
    MockSession* mockSessionPtr_ = dynamic_cast<MockSession*>(sessionPtr_.get());
};

TEST_F(TransactionFeedMockPrometheusTest, subUnsub)
{
    auto& counterTx = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"tx\"}");
    auto& counterAccount =
        makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"account\"}");
    auto& counterBook = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"book\"}");

    EXPECT_CALL(counterTx, add(1));
    EXPECT_CALL(counterTx, add(-1));
    EXPECT_CALL(counterAccount, add(1));
    EXPECT_CALL(counterAccount, add(-1));
    EXPECT_CALL(counterBook, add(1));
    EXPECT_CALL(counterBook, add(-1));

    EXPECT_CALL(*mockSessionPtr_, onDisconnect);
    testFeedPtr_->sub(sessionPtr_);
    testFeedPtr_->unsub(sessionPtr_);

    auto const account = getAccountIdWithString(kAccount1);
    EXPECT_CALL(*mockSessionPtr_, onDisconnect);
    testFeedPtr_->sub(account, sessionPtr_);
    testFeedPtr_->unsub(account, sessionPtr_);

    auto const issue1 = getIssue(kCurrency, kIssuer);
    xrpl::Book const book{xrpl::xrpIssue(), issue1, std::nullopt};
    EXPECT_CALL(*mockSessionPtr_, onDisconnect);
    testFeedPtr_->sub(book, sessionPtr_);
    testFeedPtr_->unsub(book, sessionPtr_);
}

TEST_F(TransactionFeedMockPrometheusTest, AutoDisconnect)
{
    auto& counterTx = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"tx\"}");
    auto& counterAccount =
        makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"account\"}");
    auto& counterBook = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"book\"}");

    EXPECT_CALL(counterTx, add(1));
    EXPECT_CALL(counterTx, add(-1));
    EXPECT_CALL(counterAccount, add(1));
    EXPECT_CALL(counterAccount, add(-1));
    EXPECT_CALL(counterBook, add(1));
    EXPECT_CALL(counterBook, add(-1));

    std::vector<web::SubscriptionContextInterface::OnDisconnectSlot> onDisconnectSlots;

    EXPECT_CALL(*mockSessionPtr_, onDisconnect)
        .Times(3)
        .WillRepeatedly([&onDisconnectSlots](auto const& slot) {
            onDisconnectSlots.push_back(slot);
        });
    testFeedPtr_->sub(sessionPtr_);

    auto const account = getAccountIdWithString(kAccount1);
    testFeedPtr_->sub(account, sessionPtr_);

    auto const issue1 = getIssue(kCurrency, kIssuer);
    xrpl::Book const book{xrpl::xrpIssue(), issue1, std::nullopt};
    testFeedPtr_->sub(book, sessionPtr_);

    // Emulate onDisconnect signal is called
    for (auto const& slot : onDisconnectSlots)
        slot(sessionPtr_.get());

    sessionPtr_.reset();
}
