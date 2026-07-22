#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/TransactionEntry.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <optional>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kIndex = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kTxnId = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kCurrency = "0158415500000000C1F76FF6ECB0BAC600000000";
constexpr auto kApiVersion = 2;

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
        auto const output = handler.process(boost::json::parse("{}"), Context{yield});
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
        auto const output =
            handler.process(boost::json::parse(R"JSON({"tx_hash": "123"})JSON"), Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "tx_hashMalformed");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, NonExistLedgerViaLedgerHash)
{
    // mock fetchLedgerByHash return empty
    ON_CALL(*backend_, fetchLedgerByHash(xrpl::uint256{kIndex}, _))
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "ledger_hash": "{}",
                "tx_hash": "{}"
            }})JSON",
            kIndex,
            kTxnId
        )
    );
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
    ON_CALL(*backend_, fetchLedgerBySequence)
        .WillByDefault(Return(std::optional<xrpl::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "ledger_index": "4",
                "tx_hash": "{}"
            }})JSON",
            kTxnId
        )
    );
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
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(createLedgerHeader(kIndex, 30)));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchTransaction(xrpl::uint256{kTxnId}, _))
        .WillByDefault(Return(std::optional<TransactionAndMetadata>{}));
    EXPECT_CALL(*backend_, fetchTransaction).Times(1);
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "tx_hash": "{}"
                }})JSON",
                kTxnId
            )
        );
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
    tx.metadata =
        createMetaDataForCreateOffer(kCurrency, kAccount, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kAccount, 2, 100, kCurrency, kAccount2, 200, 300)
            .getSerializer()
            .peekData();
    tx.date = 123456;
    tx.ledgerSequence = 10;
    ON_CALL(*backend_, fetchTransaction(xrpl::uint256{kTxnId}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*backend_, fetchTransaction).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(createLedgerHeader(kIndex, 30)));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "tx_hash": "{}",
                    "ledger_index": "30"
                }})JSON",
                kTxnId
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "transactionNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "Transaction not found.");
    });
}

TEST_F(RPCTransactionEntryHandlerTest, NormalPath)
{
    static constexpr auto kOutput = R"JSON({
        "metadata": {
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
            "hash": "2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08"
        },
        "ledger_index": 30,
        "ledger_hash": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322",
        "validated": true
    })JSON";

    TransactionAndMetadata tx;
    tx.metadata =
        createMetaDataForCreateOffer(kCurrency, kAccount, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kAccount, 2, 100, kCurrency, kAccount2, 200, 300)
            .getSerializer()
            .peekData();
    tx.date = 123456;
    tx.ledgerSequence = 30;
    ON_CALL(*backend_, fetchTransaction(xrpl::uint256{kTxnId}, _)).WillByDefault(Return(tx));
    EXPECT_CALL(*backend_, fetchTransaction).Times(1);

    ON_CALL(*backend_, fetchLedgerBySequence)
        .WillByDefault(Return(createLedgerHeader(kIndex, tx.ledgerSequence)));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "tx_hash": "{}",
                    "ledger_index": {}
                }})JSON",
                kTxnId,
                tx.ledgerSequence
            )
        );
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(kOutput), *output.result);
    });
}

TEST_F(RPCTransactionEntryHandlerTest, NormalPathV2)
{
    static constexpr auto kOutput = R"JSON({
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
            "Fee": "2",
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
        "ledger_index": 30,
        "ledger_hash": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322",
        "close_time_iso": "2000-01-01T00:00:00Z",
        "hash": "2E2FBAAFF767227FE4381C4BE9855986A6B9F96C62F6E443731AB36F7BBB8A08",
        "validated": true
    })JSON";

    TransactionAndMetadata tx;
    tx.metadata =
        createMetaDataForCreateOffer(kCurrency, kAccount, 100, 200, 300).getSerializer().peekData();
    tx.transaction =
        createCreateOfferTransactionObject(kAccount, 2, 100, kCurrency, kAccount2, 200, 300)
            .getSerializer()
            .peekData();
    tx.date = 123456;
    tx.ledgerSequence = 30;
    EXPECT_CALL(*backend_, fetchTransaction(xrpl::uint256{kTxnId}, _)).WillOnce(Return(tx));
    EXPECT_CALL(*backend_, fetchLedgerBySequence)
        .WillOnce(Return(createLedgerHeader(kIndex, tx.ledgerSequence)));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TransactionEntryHandler{backend_}};
        auto const req = boost::json::parse(
            fmt::format(
                R"JSON({{
                    "tx_hash": "{}",
                    "ledger_index": {}
                }})JSON",
                kTxnId,
                tx.ledgerSequence
            )
        );
        auto const output =
            handler.process(req, Context{.yield = yield, .apiVersion = kApiVersion});
        ASSERT_TRUE(output);
        EXPECT_EQ(boost::json::parse(kOutput), *output.result);
    });
}
