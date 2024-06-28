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

#include "data/Types.hpp"
#include "etl/ETLState.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/Tx.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockETLService.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

using TestTxHandler = BaseTxHandler<MockETLService>;

auto constexpr static TXNID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
auto constexpr static NFTID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF";
auto constexpr static NFTID2 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
auto constexpr static ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
auto constexpr static ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
auto constexpr static CURRENCY = "0158415500000000C1F76FF6ECB0BAC600000000";
constexpr static auto CTID = "C002807000010002";  // seq 163952 txindex 1 netid 2
constexpr static auto SEQ_FROM_CTID = 163952;
auto constexpr static DEFAULT_OUT_1 = R"({
    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
    "Fee": "2",
    "Sequence": 100,
    "SigningPubKey": "74657374",
    "TakerGets": {
        "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
        "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
        "value": "200"
    },
    "TakerPays": "300",
    "TransactionType": "OfferCreate",
    "hash": "2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08",
    "meta": {
        "AffectedNodes": [
            {
                "CreatedNode": {
                    "LedgerEntryType": "Offer",
                    "NewFields": {
                        "TakerGets": "200",
                        "TakerPays": {
                            "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                            "value": "300"
                        }
                    }
                }
            }
        ],
        "TransactionIndex": 100,
        "TransactionResult": "tesSUCCESS"
    },
    "date": 123456,
    "ledger_index": 100,
    "inLedger": 100,
    "validated": true
})";

auto constexpr static DEFAULT_OUT_2 = R"({
    "hash": "2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08",
    "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
    "ledger_index": 100,
    "meta": {
        "AffectedNodes": [
            {
                "CreatedNode": {
                    "LedgerEntryType": "Offer",
                    "NewFields": {
                        "TakerGets": "200",
                        "TakerPays": {
                            "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                            "value": "300"
                        }
                    }
                }
            }
        ],
        "TransactionIndex": 100,
        "TransactionResult": "tesSUCCESS"
    },
    "tx_json": {
        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
        "date": 123456,
        "Fee": "2",
        "ledger_index": 100,
        "Sequence": 100,
        "SigningPubKey": "74657374",
        "TakerGets": {
            "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
            "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
            "value": "200"
        },
        "TakerPays": "300",
        "TransactionType": "OfferCreate"
    },
    "close_time_iso": "2000-01-01T00:00:00Z",
    "validated": true
})";
constexpr static auto INDEX = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";

class RPCTxTest : public HandlerBaseTest {};

TEST_F(RPCTxTest, ExcessiveLgrRange)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "min_ledger": 1,
                "max_ledger": 1002
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "excessiveLgrRange");
        EXPECT_EQ(err.at("error_message").as_string(), "Ledger range exceeds 1000.");
    });
}

TEST_F(RPCTxTest, InvalidBinaryV1)
{
    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "binary": 12
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 1u});
        ASSERT_TRUE(output);
    });
}

TEST_F(RPCTxTest, InvalidBinaryV2)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "binary": 12
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2u});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid parameters.");
    });
}

TEST_F(RPCTxTest, InvalidLgrRange)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "max_ledger": 1,
                "min_ledger": 10
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidLgrRange");
        EXPECT_EQ(err.at("error_message").as_string(), "Ledger range is invalid.");
    });
}

TEST_F(RPCTxTest, TxnNotFound)
{
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _))
        .WillOnce(Return(std::optional<TransactionAndMetadata>{}));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "txnNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
    });
}

TEST_F(RPCTxTest, TxnNotFoundInGivenRangeSearchAllFalse)
{
    backend->setRange(10, 30);
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _))
        .WillOnce(Return(std::optional<TransactionAndMetadata>{}));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "min_ledger": 1,
                "max_ledger": 1000
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "txnNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
        EXPECT_EQ(err.at("searched_all").as_bool(), false);
    });
}

TEST_F(RPCTxTest, TxnNotFoundInGivenRangeSearchAllTrue)
{
    backend->setRange(1, 1000);
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _))
        .WillOnce(Return(std::optional<TransactionAndMetadata>{}));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "min_ledger": 1,
                "max_ledger": 1000
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "txnNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
        EXPECT_EQ(err.at("searched_all").as_bool(), true);
    });
}

// when ledger range and ctid are provided, searched_all should not be present, because the seq is specified in ctid
TEST_F(RPCTxTest, CtidNotFoundSearchAllFalse)
{
    backend->setRange(1, 1000);
    EXPECT_CALL(*backend, fetchAllTransactionsInLedger(SEQ_FROM_CTID, _))
        .WillOnce(Return(std::vector<TransactionAndMetadata>{}));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{2}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "ctid": "{}",
                "min_ledger": 1,
                "max_ledger": 1000
            }})",
            CTID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "txnNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
        EXPECT_FALSE(err.contains("searched_all"));
    });
}

TEST_F(RPCTxTest, DefaultParameter_API_v1)
{
    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;

    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 1u});
        ASSERT_TRUE(output);

        EXPECT_EQ(*output.result, json::parse(DEFAULT_OUT_1));
    });
}

TEST_F(RPCTxTest, PaymentTx_API_v1)
{
    TransactionAndMetadata tx;
    tx.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 2, 3, 300).getSerializer().peekData();
    tx.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;

    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 1u});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("DeliverMax"));
        EXPECT_EQ(output.result->at("Amount"), output.result->at("DeliverMax"));
    });
}

TEST_F(RPCTxTest, PaymentTx_API_v2)
{
    TransactionAndMetadata tx;
    tx.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 2, 3, 300).getSerializer().peekData();
    tx.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;

    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));
    EXPECT_CALL(*backend, fetchLedgerBySequence(tx.ledgerSequence, _)).WillOnce(Return(std::nullopt));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2u});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().contains("tx_json"));
        EXPECT_TRUE(output.result->as_object().at("tx_json").as_object().contains("DeliverMax"));
        EXPECT_FALSE(output.result->as_object().at("tx_json").as_object().contains("Amount"));
    });
}

TEST_F(RPCTxTest, DefaultParameter_API_v2)
{
    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;

    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, tx.ledgerSequence);
    EXPECT_CALL(*backend, fetchLedgerBySequence(tx.ledgerSequence, _)).WillOnce(Return(ledgerHeader));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2u});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(DEFAULT_OUT_2));
    });
}

TEST_F(RPCTxTest, ReturnBinary)
{
    // Note: `inLedger` is API v1 only. See DefaultOutput_*
    auto constexpr static OUT = R"({
        "meta": "201C00000064F8E311006FE864D50AA87BEE5380000158415500000000C1F76FF6ECB0BAC6000000004B4E9C06F24296074F7BC48F92A97916C6DC5EA96540000000000000C8E1E1F1031000",
        "tx": "120007240000006464400000000000012C65D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF368400000000000000273047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA9",
        "hash": "2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08",
        "date": 123456,
        "ledger_index": 100,
        "inLedger": 100,
        "validated": true
    })";

    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "binary": true
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

// mimic 1.12 rippled, return ctid when binary is true. This will be changed on rippled.
TEST_F(RPCTxTest, ReturnBinaryWithCTID)
{
    // Note: `inLedger` is API v1 only. See DefaultOutput_*
    auto constexpr static OUT = R"({
        "meta": "201C00000064F8E311006FE864D50AA87BEE5380000158415500000000C1F76FF6ECB0BAC6000000004B4E9C06F24296074F7BC48F92A97916C6DC5EA96540000000000000C8E1E1F1031000",
        "tx": "120007240000006464400000000000012C65D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF368400000000000000273047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA9",
        "hash": "2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08",
        "date": 123456,
        "ledger_index": 100,
        "inLedger": 100,
        "ctid": "C000006400640002",
        "validated": true
    })";

    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{.networkID = 2}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "binary": true
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

TEST_F(RPCTxTest, MintNFT)
{
    // Note: `inLedger` is API v1 only. See DefaultOutput_*
    auto static const OUT = fmt::format(
        R"({{
            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Fee": "50",
            "NFTokenTaxon": 123,
            "Sequence": 1,
            "SigningPubKey": "74657374",
            "TransactionType": "NFTokenMint",
            "hash": "C74463F49CFDCBEF3E9902672719918CDE5042DC7E7660BEBD1D1105C4B6DFF4",
            "meta": {{
                "AffectedNodes": [
                {{
                    "ModifiedNode": {{
                    "FinalFields": {{
                        "NFTokens": [
                        {{
                            "NFToken": 
                            {{
                                "NFTokenID": "{}",
                                "URI": "7465737475726C"
                            }}
                        }},
                        {{
                            "NFToken": 
                            {{
                                "NFTokenID": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                                "URI": "7465737475726C"
                            }}
                        }}
                        ]
                    }},
                    "LedgerEntryType": "NFTokenPage",
                    "PreviousFields": {{
                        "NFTokens": [
                        {{
                            "NFToken": 
                            {{
                                "NFTokenID": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                                "URI": "7465737475726C"
                            }}
                        }}
                        ]
                    }}
                    }}
                }}
                ],
                "TransactionIndex": 0,
                "TransactionResult": "tesSUCCESS",
                "nftoken_id": "{}"
            }},
            "date": 123456,
            "ledger_index": 100,
            "inLedger": 100,
            "validated": true
        }})",
        NFTID,
        NFTID
    );
    TransactionAndMetadata tx = CreateMintNFTTxWithMetadata(ACCOUNT, 1, 50, 123, NFTID);

    tx.date = 123456;
    tx.ledgerSequence = 100;
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

TEST_F(RPCTxTest, NFTAcceptOffer)
{
    TransactionAndMetadata tx = CreateAcceptNFTBuyerOfferTxWithMetadata(ACCOUNT, 1, 50, NFTID, INDEX);

    tx.date = 123456;
    tx.ledgerSequence = 100;
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("meta").at("nftoken_id").as_string(), NFTID);
    });
}

TEST_F(RPCTxTest, NFTCancelOffer)
{
    std::vector<std::string> ids{NFTID, NFTID2};
    TransactionAndMetadata tx = CreateCancelNFTOffersTxWithMetadata(ACCOUNT, 1, 50, ids);

    tx.date = 123456;
    tx.ledgerSequence = 100;
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this, &ids](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);

        for (auto const& id : output.result->at("meta").at("nftoken_ids").as_array()) {
            auto const idStr = id.as_string();
            auto const it = std::find(ids.begin(), ids.end(), idStr);
            ASSERT_NE(it, ids.end()) << "Unexpected NFT ID: " << idStr;
            ids.erase(it);
        }

        EXPECT_TRUE(ids.empty());
    });
}

TEST_F(RPCTxTest, NFTCreateOffer)
{
    TransactionAndMetadata tx = CreateCreateNFTOfferTxWithMetadata(ACCOUNT, 1, 50, NFTID, 123, NFTID2);

    tx.date = 123456;
    tx.ledgerSequence = 100;
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->at("meta").at("offer_id").as_string() == NFTID2);
    });
}

TEST_F(RPCTxTest, CTIDAndTransactionBothProvided)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "ctid": "{}"
            }})",
            TXNID,
            CTID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid parameters.");
    });
}

TEST_F(RPCTxTest, CTIDAndTransactionBothNotProvided)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(R"({ "command": "tx"})");
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid parameters.");
    });
}

TEST_F(RPCTxTest, CTIDInvalidType)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(R"({ "command": "tx", "ctid": 123})");
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid parameters.");
    });
}

TEST_F(RPCTxTest, CTIDInvalidString)
{
    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{.networkID = 5}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(R"({ "command": "tx", "ctid": "B002807000010002"})");
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid parameters.");
    });
}

TEST_F(RPCTxTest, CTIDNotMatch)
{
    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{.networkID = 5}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "ctid": "{}"
            }})",
            CTID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error_code").as_uint64(), 4);
        EXPECT_EQ(
            err.at("error_message").as_string(),
            "Wrong network. You should submit this request to a node running on NetworkID: 2"
        );
    });
}

TEST_F(RPCTxTest, ReturnCTIDForTxInput)
{
    auto constexpr static OUT = R"({
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Fee":"2",
            "Sequence":100,
            "SigningPubKey":"74657374",
            "TakerGets":
            {
                "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "value":"200"
            },
            "ctid":"C000006400640002",
            "TakerPays":"300",
            "TransactionType":"OfferCreate",
            "hash":"2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08",
            "meta":
            {
                "AffectedNodes":
                [
                    {
                        "CreatedNode":
                        {
                            "LedgerEntryType":"Offer",
                            "NewFields":
                            {
                                "TakerGets":"200",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "value":"300"
                                }
                            }
                        }
                    }
                ],
                "TransactionIndex":100,
                "TransactionResult":"tesSUCCESS"
            },
            "date":123456,
            "ledger_index":100,
            "inLedger":100,
            "validated": true
    })";

    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{.networkID = 2}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

TEST_F(RPCTxTest, NotReturnCTIDIfETLNotAvaiable)
{
    auto constexpr static OUT = R"({
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Fee":"2",
            "Sequence":100,
            "SigningPubKey":"74657374",
            "TakerGets":
            {
                "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "value":"200"
            },
            "TakerPays":"300",
            "TransactionType":"OfferCreate",
            "hash":"2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08",
            "meta":
            {
                "AffectedNodes":
                [
                    {
                        "CreatedNode":
                        {
                            "LedgerEntryType":"Offer",
                            "NewFields":
                            {
                                "TakerGets":"200",
                                "TakerPays":
                                {
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "value":"300"
                                }
                            }
                        }
                    }
                ],
                "TransactionIndex":100,
                "TransactionResult":"tesSUCCESS"
            },
            "date":123456,
            "ledger_index":100,
            "inLedger":100,
            "validated": true
    })";

    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256{TXNID}, _)).WillOnce(Return(tx));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(std::nullopt));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

TEST_F(RPCTxTest, ViaCTID)
{
    auto static const OUT = fmt::format(
        R"({{
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Fee":"2",
            "Sequence":100,
            "SigningPubKey":"74657374",
            "TakerGets":
            {{
                "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "value":"200"
            }},
            "ctid":"{}",
            "TakerPays":"300",
            "TransactionType":"OfferCreate",
            "hash":"2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08",
            "meta":
            {{
                "AffectedNodes":
                [
                    {{
                        "CreatedNode":
                        {{
                            "LedgerEntryType":"Offer",
                            "NewFields":
                            {{
                                "TakerGets":"200",
                                "TakerPays":
                                {{
                                    "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                                    "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "value":"300"
                                }}
                            }}
                        }}
                    }}
                ],
                "TransactionIndex":1,
                "TransactionResult":"tesSUCCESS"
            }},
            "date":123456,
            "ledger_index":{},
            "inLedger":{},
            "validated": true
    }})",
        CTID,
        SEQ_FROM_CTID,
        SEQ_FROM_CTID
    );

    TransactionAndMetadata tx1;
    tx1.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 1, 200, 300).getSerializer().peekData();
    tx1.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx1.date = 123456;
    tx1.ledgerSequence = SEQ_FROM_CTID;

    TransactionAndMetadata tx2;
    tx2.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 2, 3, 300).getSerializer().peekData();
    tx2.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30).getSerializer().peekData();
    tx2.ledgerSequence = SEQ_FROM_CTID;

    EXPECT_CALL(*backend, fetchAllTransactionsInLedger(SEQ_FROM_CTID, _)).WillOnce(Return(std::vector{tx1, tx2}));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{.networkID = 2}));

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "ctid": "{}"
            }})",
            CTID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(OUT));
    });
}

TEST_F(RPCTxTest, ViaLowercaseCTID)
{
    TransactionAndMetadata tx1;
    tx1.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 1, 200, 300).getSerializer().peekData();
    tx1.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx1.date = 123456;
    tx1.ledgerSequence = SEQ_FROM_CTID;

    TransactionAndMetadata tx2;
    tx2.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 2, 3, 300).getSerializer().peekData();
    tx2.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30).getSerializer().peekData();
    tx2.ledgerSequence = SEQ_FROM_CTID;

    EXPECT_CALL(*backend, fetchAllTransactionsInLedger(SEQ_FROM_CTID, _)).WillOnce(Return(std::vector{tx1, tx2}));

    auto const rawETLPtr = dynamic_cast<MockETLService*>(mockETLServicePtr.get());
    ASSERT_NE(rawETLPtr, nullptr);
    EXPECT_CALL(*rawETLPtr, getETLState).WillOnce(Return(etl::ETLState{.networkID = 2}));

    std::string ctid(CTID);
    std::transform(ctid.begin(), ctid.end(), ctid.begin(), ::tolower);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TestTxHandler{backend, mockETLServicePtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "ctid": "{}"
            }})",
            ctid
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ctid").as_string(), CTID);
    });
}
