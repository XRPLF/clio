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
#include <rpc/handlers/Tx.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto TXNID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr static auto NFTID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF";
constexpr static auto NFTID2 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA";
constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto CURRENCY = "0158415500000000C1F76FF6ECB0BAC600000000";

class RPCTxTest : public HandlerBaseTest
{
};

TEST_F(RPCTxTest, ExcessiveLgrRange)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "min_ledger": 1,
                "max_ledger":1002
                }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "excessiveLgrRange");
        EXPECT_EQ(err.at("error_message").as_string(), "Ledger range exceeds 1000.");
    });
}

TEST_F(RPCTxTest, InvalidLgrRange)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "max_ledger": 1,
                "min_ledger": 10
                }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidLgrRange");
        EXPECT_EQ(err.at("error_message").as_string(), "Ledger range is invalid.");
    });
}

TEST_F(RPCTxTest, TxnNotFound)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _))
        .WillByDefault(Return(std::optional<TransactionAndMetadata>{}));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
                }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "txnNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
    });
}

TEST_F(RPCTxTest, TxnNotFoundInGivenRangeSearchAllFalse)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(10);  // min
    mockBackendPtr->updateRange(30);  // max
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _))
        .WillByDefault(Return(std::optional<TransactionAndMetadata>{}));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "min_ledger": 1,
                "max_ledger":1000
                }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "txnNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
        EXPECT_EQ(err.at("searched_all").as_bool(), false);
    });
}

TEST_F(RPCTxTest, TxnNotFoundInGivenRangeSearchAllTrue)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(1);     // min
    mockBackendPtr->updateRange(1000);  // max
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _))
        .WillByDefault(Return(std::optional<TransactionAndMetadata>{}));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "min_ledger": 1,
                "max_ledger":1000
                }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "txnNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
        EXPECT_EQ(err.at("searched_all").as_bool(), true);
    });
}

TEST_F(RPCTxTest, DefaultParameter)
{
    auto constexpr static OUT = R"({
            "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "Fee":"2",
            "Sequence":100,
            "SigningPubKey":"74657374",
            "TakerGets":{
                "currency":"0158415500000000C1F76FF6ECB0BAC600000000",
                "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                "value":"200"
            },
            "TakerPays":"300",
            "TransactionType":"OfferCreate",
            "hash":"2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08",
            "meta":{
                "AffectedNodes":[
                    {
                        "CreatedNode":{
                        "LedgerEntryType":"Offer",
                        "NewFields":{
                            "TakerGets":"200",
                            "TakerPays":{
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
            "validated": true
    })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
                }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(OUT));
    });
}

TEST_F(RPCTxTest, ReturnBinary)
{
    auto constexpr static OUT = R"({
        "meta":"201C00000064F8E311006FE864D50AA87BEE5380000158415500000000C1F76FF6ECB0BAC6000000004B4E9C06F24296074F7BC48F92A97916C6DC5EA96540000000000000C8E1E1F1031000",
        "tx":"120007240000006464400000000000012C65D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF368400000000000000273047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA9",
        "hash":"05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD",
        "date":123456,
        "ledger_index":100,
        "validated": true
    })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = 100;
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}",
                "binary": true
                }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(OUT));
    });
}

TEST_F(RPCTxTest, MintNFT)
{
    auto const static OUT = fmt::format(
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
            "validated": true,
            "date": 123456,
            "ledger_index": 100
        }})",
        NFTID,
        NFTID);
    TransactionAndMetadata tx = CreateMintNFTTxWithMetadata(ACCOUNT, 1, 50, 123, NFTID);
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    tx.date = 123456;
    tx.ledgerSequence = 100;
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(OUT));
    });
}

TEST_F(RPCTxTest, NFTAcceptOffer)
{
    TransactionAndMetadata tx = CreateAcceptNFTOfferTxWithMetadata(ACCOUNT, 1, 50, NFTID);
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    tx.date = 123456;
    tx.ledgerSequence = 100;
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("meta").at("nftoken_id").as_string(), NFTID);
    });
}

TEST_F(RPCTxTest, NFTCancelOffer)
{
    std::vector<std::string> ids{NFTID, NFTID2};
    TransactionAndMetadata tx = CreateCancelNFTOffersTxWithMetadata(ACCOUNT, 1, 50, ids);
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    tx.date = 123456;
    tx.ledgerSequence = 100;
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);
    runSpawn([this, &ids](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);

        for (auto const& id : output->at("meta").at("nftoken_ids").as_array())
        {
            auto const idStr = id.as_string();
            ids.erase(std::find(ids.begin(), ids.end(), std::string{idStr.c_str(), idStr.size()}));
        }

        EXPECT_TRUE(ids.empty());
    });
}

TEST_F(RPCTxTest, NFTCreateOffer)
{
    TransactionAndMetadata tx = CreateCreateNFTOfferTxWithMetadata(ACCOUNT, 1, 50, NFTID, 123, NFTID2);
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    tx.date = 123456;
    tx.ledgerSequence = 100;
    ON_CALL(*rawBackendPtr, fetchTransaction(ripple::uint256{TXNID}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*rawBackendPtr, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TxHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{ 
                "command": "tx",
                "transaction": "{}"
            }})",
            TXNID));
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->at("meta").at("offer_id").as_string() == NFTID2);
    });
}
