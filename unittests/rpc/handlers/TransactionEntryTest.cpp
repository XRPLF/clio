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

#include <rpc/common/AnyHandler.h>
#include <rpc/handlers/TransactionEntry.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto INDEX = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr static auto TXNID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto CURRENCY = "0158415500000000C1F76FF6ECB0BAC600000000";

class RPCTransactionEntryHandlerTest : public HandlerBaseTest
{
};

TEST_F(RPCTransactionEntryHandlerTest, TxHashNotProvide)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{mockBackendPtr}};
        auto const output = handler.process(json::parse("{}"), Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "fieldNotFoundTransaction");
        EXPECT_EQ(err.at("error_message").as_string(), "Missing field.");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, TxHashWrongFormat)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{mockBackendPtr}};
        auto const output = handler.process(json::parse(R"({"tx_hash":"123"})"), Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "tx_hashMalformed");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, NonExistLedgerViaLedgerHash)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // mock fetchLedgerByHash return empty
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{INDEX}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "ledger_hash": "{}",
            "tx_hash": "{}"
        }})",
        INDEX,
        TXNID));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCTransactionEntryHandlerTest, NonExistLedgerViaLedgerIndex)
{
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    // mock fetchLedgerBySequence return empty
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "ledger_index": "4",
            "tx_hash": "{}"
        }})",
        TXNID));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{mockBackendPtr}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, TXNotFound)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(CreateLedgerInfo(INDEX, 30)));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _))
        .WillByDefault(Return(std::optional<TransactionAndMetadata>{}));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "tx_hash": "{}"
            }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "transactionNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, LedgerSeqNotMatch)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 10;
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);

    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(CreateLedgerInfo(INDEX, 30)));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "tx_hash": "{}",
                "ledger_index": "30"
            }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "transactionNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, NormalPath)
{
    static auto constexpr OUTPUT = R"({
                                        "metadata":{
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
                                        "tx_json":
                                        {
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
                                            "hash":"2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08"
                                        },
                                        "ledger_index":30,
                                        "ledger_hash":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322",
                                        "validated":true
                                    })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 30;
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);

    mockBackendPtr->updateRange(10);                 // min
    mockBackendPtr->updateRange(tx.ledgerSequence);  // max
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(CreateLedgerInfo(INDEX, tx.ledgerSequence)));
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "tx_hash": "{}",
                "ledger_index": {}
            }})",
            TXNID,
            tx.ledgerSequence));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(OUTPUT), *output);
    });
}
