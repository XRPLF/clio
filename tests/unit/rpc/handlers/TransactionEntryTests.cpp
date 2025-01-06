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
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/TransactionEntry.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <optional>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kINDEX = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kTXN_ID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kCURRENCY = "0158415500000000C1F76FF6ECB0BAC600000000";

}  // namespace

struct RPCTransactionEntryHandlerTest : HandlerBaseTest {
    RPCTransactionEntryHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

TEST_F(RPCTransactionEntryHandlerTest, TxHashNotProvide)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const output = handler.process(json::parse("{}"), Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "fieldNotFoundTransaction");
        EXPECT_EQ(err.at("error_message").as_string(), "Missing field.");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, TxHashWrongFormat)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const output = handler.process(json::parse(R"({"tx_hash":"123"})"), Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "tx_hashMalformed");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, NonExistLedgerViaLedgerHash)
{
    // mock fetchLedgerByHash return empty
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kINDEX}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "ledger_hash": "{}",
            "tx_hash": "{}"
        }})",
        kINDEX,
        kTXN_ID
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCTransactionEntryHandlerTest, NonExistLedgerViaLedgerIndex)
{
    // mock fetchLedgerBySequence return empty
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "ledger_index": "4",
            "tx_hash": "{}"
        }})",
        kTXN_ID
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, TXNotFound)
{
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(createLedgerHeader(kINDEX, 30)));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchTransaction(ripple::uint256{kTXN_ID}, _))
        .WillByDefault(Return(std::optional<TransactionAndMetadata>{}));
    EXPECT_CALL(*backend_, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "tx_hash": "{}"
            }})",
            kTXN_ID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "transactionNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, LedgerSeqNotMatch)
{
    TransactionAndMetadata tx;
    tx.metadata = createMetaDataForCreateOffer(kCURRENCY, kACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kACCOUNT, 2, 100, kCURRENCY, kACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 10;
    ON_CALL(*backend_, fetchTransaction(ripple::uint256{kTXN_ID}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*backend_, fetchTransaction).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(createLedgerHeader(kINDEX, 30)));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "tx_hash": "{}",
                "ledger_index": "30"
            }})",
            kTXN_ID
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "transactionNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, NormalPath)
{
    static constexpr auto kOUTPUT = R"({
                                        "metadata":
                                        {
                                            "AffectedNodes":
                                            [
                                                {
                                                    "CreatedNode":
                                                    {
                                                        "LedgerEntryType": "Offer",
                                                        "NewFields":
                                                        {
                                                            "TakerGets": "200",
                                                            "TakerPays":
                                                            {
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
                                        "tx_json":
                                        {
                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "Fee": "2",
                                            "Sequence": 100,
                                            "SigningPubKey": "74657374",
                                            "TakerGets":
                                            {
                                                "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                                                "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                "value": "200"
                                            },
                                            "TakerPays": "300",
                                            "TransactionType": "OfferCreate",
                                            "hash": "2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08"
                                        },
                                        "ledger_index": 30,
                                        "ledger_hash": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322",
                                        "validated": true
                                    })";

    TransactionAndMetadata tx;
    tx.metadata = createMetaDataForCreateOffer(kCURRENCY, kACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kACCOUNT, 2, 100, kCURRENCY, kACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 30;
    ON_CALL(*backend_, fetchTransaction(ripple::uint256{kTXN_ID}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*backend_, fetchTransaction).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(createLedgerHeader(kINDEX, tx.ledgerSequence)));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "tx_hash": "{}",
                "ledger_index": {}
            }})",
            kTXN_ID,
            tx.ledgerSequence
        ));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(kOUTPUT), *output.result);
    });
}

TEST_F(RPCTransactionEntryHandlerTest, NormalPathV2)
{
    static constexpr auto kOUTPUT = R"({
                                        "meta":
                                        {
                                            "AffectedNodes":
                                            [
                                                {
                                                    "CreatedNode":
                                                    {
                                                        "LedgerEntryType": "Offer",
                                                        "NewFields":
                                                        {
                                                            "TakerGets": "200",
                                                            "TakerPays":
                                                            {
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
                                        "tx_json":
                                        {
                                            "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "Fee": "2",
                                            "Sequence": 100,
                                            "SigningPubKey": "74657374",
                                            "TakerGets":
                                            {
                                                "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                                                "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                "value": "200"
                                            },
                                            "TakerPays": "300",
                                            "TransactionType": "OfferCreate"
                                        },
                                        "ledger_index": 30,
                                        "ledger_hash": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322",
                                        "close_time_iso": "2000-01-01T00:00:00Z",
                                        "hash": "2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08",
                                        "validated": true
                                    })";

    TransactionAndMetadata tx;
    tx.metadata = createMetaDataForCreateOffer(kCURRENCY, kACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kACCOUNT, 2, 100, kCURRENCY, kACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 30;
    EXPECT_CALL(*backend_, fetchTransaction(ripple::uint256{kTXN_ID}, _)).WillOnce(Return(tx));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).WillOnce(Return(createLedgerHeader(kINDEX, tx.ledgerSequence)));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "tx_hash": "{}",
                "ledger_index": {}
            }})",
            kTXN_ID,
            tx.ledgerSequence
        ));
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = 2});
        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(kOUTPUT), *output.result);
    });
}
