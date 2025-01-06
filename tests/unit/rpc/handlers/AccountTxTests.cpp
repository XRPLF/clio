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
#include "rpc/handlers/AccountTx.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/STObject.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kMIN_SEQ = 10;
constexpr auto kMAX_SEQ = 30;
constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kNFT_ID = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF";
constexpr auto kNFT_ID2 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA";
constexpr auto kNFT_ID3 = "15FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF";

}  // namespace

struct RPCAccountTxHandlerTest : HandlerBaseTest {
    RPCAccountTxHandlerTest()
    {
        backend_->setRange(kMIN_SEQ, kMAX_SEQ);
    }
};

struct AccountTxParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::optional<std::string> expectedError;
    std::optional<std::string> expectedErrorMessage;
    std::uint32_t apiVersion = 2u;
};

// parameterized test cases for parameters check
struct AccountTxParameterTest : public RPCAccountTxHandlerTest,
                                public WithParamInterface<AccountTxParamTestCaseBundle> {
    static auto
    generateTestValuesForParametersTest()
    {
        return std::vector<AccountTxParamTestCaseBundle>{
            AccountTxParamTestCaseBundle{
                .testName = "MissingAccount",
                .testJson = R"({})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Required field 'account' missing"
            },
            AccountTxParamTestCaseBundle{
                .testName = "BinaryNotBool",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "binary": 1})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Invalid parameters."
            },
            AccountTxParamTestCaseBundle{
                .testName = "BinaryNotBool_API_v1",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "binary": 1})",
                .expectedError = std::nullopt,
                .expectedErrorMessage = std::nullopt,
                .apiVersion = 1u
            },
            AccountTxParamTestCaseBundle{
                .testName = "ForwardNotBool",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "forward": 1})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Invalid parameters."
            },
            AccountTxParamTestCaseBundle{
                .testName = "ForwardNotBool_API_v1",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "forward": 1})",
                .expectedError = std::nullopt,
                .expectedErrorMessage = std::nullopt,
                .apiVersion = 1u
            },
            AccountTxParamTestCaseBundle{
                .testName = "ledger_index_minNotInt",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index_min": "x"})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Invalid parameters."
            },
            AccountTxParamTestCaseBundle{
                .testName = "ledger_index_maxNotInt",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index_max": "x"})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Invalid parameters."
            },
            AccountTxParamTestCaseBundle{
                .testName = "ledger_indexInvalid",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": "x"})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "ledgerIndexMalformed"
            },
            AccountTxParamTestCaseBundle{
                .testName = "ledger_hashInvalid",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": "x"})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "ledger_hashMalformed"
            },
            AccountTxParamTestCaseBundle{
                .testName = "ledger_hashNotString",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": 123})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "ledger_hashNotString"
            },
            AccountTxParamTestCaseBundle{
                .testName = "limitNotInt",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": "123"})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Invalid parameters."
            },
            AccountTxParamTestCaseBundle{
                .testName = "limitNegative",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": -1})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Invalid parameters."
            },
            AccountTxParamTestCaseBundle{
                .testName = "limitZero",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": 0})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Invalid parameters."
            },
            AccountTxParamTestCaseBundle{
                .testName = "MarkerNotObject",
                .testJson = R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "marker": 101})",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "invalidMarker"
            },
            AccountTxParamTestCaseBundle{
                .testName = "MarkerMissingSeq",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "marker": {"ledger": 123}
            })",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Required field 'seq' missing"
            },
            AccountTxParamTestCaseBundle{
                .testName = "MarkerMissingLedger",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "marker": {"seq": 123}
            })",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Required field 'ledger' missing"
            },
            AccountTxParamTestCaseBundle{
                .testName = "MarkerLedgerNotInt",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "marker": 
                {
                    "seq": "string",
                    "ledger": 1
                }
            })",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Invalid parameters."
            },
            AccountTxParamTestCaseBundle{
                .testName = "MarkerSeqNotInt",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "marker": 
                {
                    "ledger": "string",
                    "seq": 1
                }
            })",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Invalid parameters."
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMinLessThanMinSeq",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_min": 9
            })",
                .expectedError = "lgrIdxMalformed",
                .expectedErrorMessage = "ledgerSeqMinOutOfRange"
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxLargeThanMaxSeq",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 31
            })",
                .expectedError = "lgrIdxMalformed",
                .expectedErrorMessage = "ledgerSeqMaxOutOfRange"
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxLargeThanMaxSeq_API_v1",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 31
            })",
                .expectedError = std::nullopt,
                .expectedErrorMessage = std::nullopt,
                .apiVersion = 1u
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxSmallerThanMinSeq",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 9
            })",
                .expectedError = "lgrIdxMalformed",
                .expectedErrorMessage = "ledgerSeqMaxOutOfRange"
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxSmallerThanMinSeq_API_v1",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 9
            })",
                .expectedError = "lgrIdxsInvalid",
                .expectedErrorMessage = "Ledger indexes invalid.",
                .apiVersion = 1u
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMinSmallerThanMinSeq",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_min": 9
            })",
                .expectedError = "lgrIdxMalformed",
                .expectedErrorMessage = "ledgerSeqMinOutOfRange"
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMinSmallerThanMinSeq_API_v1",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_min": 9
            })",
                .expectedError = std::nullopt,
                .expectedErrorMessage = std::nullopt,
                .apiVersion = 1u
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMinLargerThanMaxSeq",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_min": 31
            })",
                .expectedError = "lgrIdxMalformed",
                .expectedErrorMessage = "ledgerSeqMinOutOfRange"
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMinLargerThanMaxSeq_API_v1",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_min": 31
            })",
                .expectedError = "lgrIdxsInvalid",
                .expectedErrorMessage = "Ledger indexes invalid.",
                .apiVersion = 1u
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxLessThanLedgerIndexMin",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 11,
                "ledger_index_min": 20
            })",
                .expectedError = "invalidLgrRange",
                .expectedErrorMessage = "Ledger range is invalid."
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxLessThanLedgerIndexMin_API_v1",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 11,
                "ledger_index_min": 20
            })",
                .expectedError = "lgrIdxsInvalid",
                .expectedErrorMessage = "Ledger indexes invalid.",
                .apiVersion = 1u
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxMinAndLedgerIndex",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_index_max": 20,
                "ledger_index_min": 11,
                "ledger_index": 10
            })",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "containsLedgerSpecifierAndRange"
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxMinAndLedgerIndexValidated",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 20,
                "ledger_index_min": 11,
                "ledger_index": "validated"
            })",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "containsLedgerSpecifierAndRange"
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxMinAndLedgerIndex_API_v1",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 20,
                "ledger_index_min": 11,
                "ledger_index": 10
            })",
                .expectedError = std::nullopt,
                .expectedErrorMessage = std::nullopt,
                .apiVersion = 1u
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxMinAndLedgerHash",
                .testJson = fmt::format(
                    R"({{
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_index_max": 20,
                "ledger_index_min": 11,
                "ledger_hash": "{}"
            }})",
                    kLEDGER_HASH
                ),
                .expectedError = "invalidParams",
                .expectedErrorMessage = "containsLedgerSpecifierAndRange"
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxMinAndLedgerHash_API_v1",
                .testJson = fmt::format(
                    R"({{
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_index_max": 20,
                "ledger_index_min": 11,
                "ledger_hash": "{}"
            }})",
                    kLEDGER_HASH
                ),
                .expectedError = std::nullopt,
                .expectedErrorMessage = std::nullopt,
                .apiVersion = 1u
            },
            AccountTxParamTestCaseBundle{
                .testName = "LedgerIndexMaxMinAndLedgerIndexValidated_API_v1",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 20,
                "ledger_index_min": 11,
                "ledger_index": "validated"
            })",
                .expectedError = std::nullopt,
                .expectedErrorMessage = std::nullopt,
                .apiVersion = 1u
            },
            AccountTxParamTestCaseBundle{
                .testName = "InvalidTxType",
                .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "tx_type": "unknow"
            })",
                .expectedError = "invalidParams",
                .expectedErrorMessage = "Invalid field 'tx_type'."
            }
        };
    };
};

INSTANTIATE_TEST_CASE_P(
    RPCAccountTxGroup1,
    AccountTxParameterTest,
    ValuesIn(AccountTxParameterTest::generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(AccountTxParameterTest, CheckParams)
{
    auto const& testBundle = GetParam();

    auto const req = json::parse(testBundle.testJson);
    if (testBundle.expectedError.has_value()) {
        ASSERT_TRUE(testBundle.expectedErrorMessage.has_value());

        runSpawn([&, this](auto yield) {
            auto const handler = AnyHandler{AccountTxHandler{backend_}};
            auto const output = handler.process(req, Context{.yield = yield, .apiVersion = testBundle.apiVersion});
            ASSERT_FALSE(output);
            auto const err = rpc::makeError(output.result.error());
            EXPECT_EQ(err.at("error").as_string(), *testBundle.expectedError);
            EXPECT_EQ(err.at("error_message").as_string(), *testBundle.expectedErrorMessage);
        });
    } else {
        EXPECT_CALL(*backend_, fetchAccountTransactions);

        runSpawn([&, this](auto yield) {
            auto const handler = AnyHandler{AccountTxHandler{backend_}};
            auto const output = handler.process(req, Context{.yield = yield, .apiVersion = testBundle.apiVersion});
            EXPECT_TRUE(output);
        });
    }
}

namespace {

std::vector<TransactionAndMetadata>
genTransactions(uint32_t seq1, uint32_t seq2)
{
    auto transactions = std::vector<TransactionAndMetadata>{};
    auto trans1 = TransactionAndMetadata();
    ripple::STObject const obj = createPaymentTransactionObject(kACCOUNT, kACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = seq1;
    ripple::STObject const metaObj = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, 22, 23);
    trans1.metadata = metaObj.getSerializer().peekData();
    trans1.date = 1;
    transactions.push_back(trans1);

    auto trans2 = TransactionAndMetadata();
    ripple::STObject const obj2 = createPaymentTransactionObject(kACCOUNT, kACCOUNT2, 1, 1, 32);
    trans2.transaction = obj.getSerializer().peekData();
    trans2.ledgerSequence = seq2;
    ripple::STObject const metaObj2 = createPaymentTransactionMetaObject(kACCOUNT, kACCOUNT2, 22, 23);
    trans2.metadata = metaObj2.getSerializer().peekData();
    trans2.date = 2;
    transactions.push_back(trans2);
    return transactions;
}

std::vector<TransactionAndMetadata>
genNFTTransactions(uint32_t seq)
{
    auto transactions = std::vector<TransactionAndMetadata>{};

    auto trans1 = createMintNftTxWithMetadata(kACCOUNT, 1, 50, 123, kNFT_ID);
    trans1.ledgerSequence = seq;
    trans1.date = 1;
    transactions.push_back(trans1);

    auto trans2 = createAcceptNftOfferTxWithMetadata(kACCOUNT, 1, 50, kNFT_ID2);
    trans2.ledgerSequence = seq;
    trans2.date = 2;
    transactions.push_back(trans2);

    auto trans3 = createCancelNftOffersTxWithMetadata(kACCOUNT, 1, 50, std::vector<std::string>{kNFT_ID2, kNFT_ID3});
    trans3.ledgerSequence = seq;
    trans3.date = 3;
    transactions.push_back(trans3);

    auto trans4 = createCreateNftOfferTxWithMetadata(kACCOUNT, 1, 50, kNFT_ID, 123, kNFT_ID2);
    trans4.ledgerSequence = seq;
    trans4.date = 4;
    transactions.push_back(trans4);
    return transactions;
}
}  // namespace

TEST_F(RPCAccountTxHandlerTest, IndexSpecificForwardTrue)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            true,
            testing::Optional(testing::Eq(TransactionsCursor{kMIN_SEQ, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": true
            }})",
            kACCOUNT,
            kMIN_SEQ + 1,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ + 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 1);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger": 12, "seq": 34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCAccountTxHandlerTest, IndexSpecificForwardFalse)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            kACCOUNT,
            kMIN_SEQ + 1,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ + 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 1);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger": 12, "seq": 34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCAccountTxHandlerTest, IndexNotSpecificForwardTrue)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            true,
            testing::Optional(testing::Eq(TransactionsCursor{kMIN_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": true
            }})",
            kACCOUNT,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger": 12, "seq": 34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCAccountTxHandlerTest, IndexNotSpecificForwardFalse)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            kACCOUNT,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger": 12, "seq": 34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCAccountTxHandlerTest, BinaryTrue)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "binary": true
            }})",
            kACCOUNT,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger": 12, "seq": 34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_EQ(
            output.result->at("transactions").as_array()[0].as_object().at("meta").as_string(),
            "201C00000000F8E5110061E762400000000000001681144B4E9C06F24296074F7B"
            "C48F92A97916C6DC5EA9E1E1E5110061E76240000000000000178114D31252CF90"
            "2EF8DD8451243869B38667CBD89DF3E1E1F1031000"
        );
        EXPECT_EQ(
            output.result->at("transactions").as_array()[0].as_object().at("tx_blob").as_string(),
            "120000240000002061400000000000000168400000000000000173047465737481"
            "144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451"
            "243869B38667CBD89DF3"
        );
        EXPECT_FALSE(output.result->at("transactions").as_array()[0].as_object().contains("date"));
        EXPECT_FALSE(output.result->at("transactions").as_array()[0].as_object().contains("inLedger"));
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCAccountTxHandlerTest, BinaryTrueV2)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ, INT32_MAX})),
            testing::_
        )
    )
        .WillOnce(Return(transCursor));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "binary": true
            }})",
            kACCOUNT,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{.yield = yield, .apiVersion = 2u});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger": 12, "seq": 34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
        EXPECT_EQ(
            output.result->at("transactions").as_array()[0].as_object().at("meta_blob").as_string(),
            "201C00000000F8E5110061E762400000000000001681144B4E9C06F24296074F7B"
            "C48F92A97916C6DC5EA9E1E1E5110061E76240000000000000178114D31252CF90"
            "2EF8DD8451243869B38667CBD89DF3E1E1F1031000"
        );
        EXPECT_EQ(
            output.result->at("transactions").as_array()[0].as_object().at("tx_blob").as_string(),
            "120000240000002061400000000000000168400000000000000173047465737481"
            "144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451"
            "243869B38667CBD89DF3"
        );
        EXPECT_FALSE(output.result->at("transactions").as_array()[0].as_object().contains("date"));
        EXPECT_FALSE(output.result->at("transactions").as_array()[0].as_object().contains("inLedger"));
        EXPECT_FALSE(output.result->as_object().contains("limit"));
    });
}

TEST_F(RPCAccountTxHandlerTest, LimitAndMarker)
{
    auto const transactions = genTransactions(kMIN_SEQ + 1, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_, testing::_, false, testing::Optional(testing::Eq(TransactionsCursor{10, 11})), testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "limit": 2,
                "forward": false,
                "marker": {{"ledger":10,"seq":11}}
            }})",
            kACCOUNT,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("limit").as_uint64(), 2);
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger": 12, "seq": 34})"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 2);
    });
}

TEST_F(RPCAccountTxHandlerTest, SpecificLedgerIndex)
{
    // adjust the order for forward->false
    auto const transactions = genTransactions(kMAX_SEQ - 1, kMIN_SEQ + 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ - 1);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ - 1, _)).WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index": {}
            }})",
            kACCOUNT,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMAX_SEQ - 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 1);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_FALSE(output.result->as_object().contains("marker"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCAccountTxHandlerTest, SpecificNonexistLedgerIntIndex)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ - 1, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index": {}
            }})",
            kACCOUNT,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountTxHandlerTest, SpecificNonexistLedgerStringIndex)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ - 1, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index": "{}"
            }})",
            kACCOUNT,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountTxHandlerTest, SpecificLedgerHash)
{
    // adjust the order for forward->false
    auto const transactions = genTransactions(kMAX_SEQ - 1, kMIN_SEQ + 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ - 1);
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_hash": "{}"
            }})",
            kACCOUNT,
            kLEDGER_HASH
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMAX_SEQ - 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 1);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_FALSE(output.result->as_object().contains("marker"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCAccountTxHandlerTest, SpecificLedgerIndexValidated)
{
    // adjust the order for forward->false
    auto const transactions = genTransactions(kMAX_SEQ, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    ON_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ, _)).WillByDefault(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index": "validated"
            }})",
            kACCOUNT
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMAX_SEQ);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_FALSE(output.result->as_object().contains("marker"));
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCAccountTxHandlerTest, TxLessThanMinSeq)
{
    auto const transactions = genTransactions(kMAX_SEQ - 1, kMIN_SEQ + 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 1, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            kACCOUNT,
            kMIN_SEQ + 2,
            kMAX_SEQ - 1
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ + 2);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 1);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });
}

TEST_F(RPCAccountTxHandlerTest, TxLargerThanMaxSeq)
{
    auto const transactions = genTransactions(kMAX_SEQ - 1, kMIN_SEQ + 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{kMAX_SEQ - 2, INT32_MAX})),
            testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            kACCOUNT,
            kMIN_SEQ + 1,
            kMAX_SEQ - 2
        ));
        auto const output = handler.process(kINPUT, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("account").as_string(), kACCOUNT);
        EXPECT_EQ(output.result->at("ledger_index_min").as_uint64(), kMIN_SEQ + 1);
        EXPECT_EQ(output.result->at("ledger_index_max").as_uint64(), kMAX_SEQ - 2);
        EXPECT_EQ(output.result->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_EQ(output.result->at("marker").as_object(), json::parse(R"({"ledger": 12, "seq": 34})"));
    });
}

TEST_F(RPCAccountTxHandlerTest, NFTTxs_API_v1)
{
    auto const out = R"({
            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "ledger_index_min": 10,
            "ledger_index_max": 30,
            "transactions": [
                {
                    "meta": {
                        "AffectedNodes": 
                        [
                            {
                                "ModifiedNode": 
                                {
                                    "FinalFields": 
                                    {
                                        "NFTokens": 
                                        [
                                            {
                                                "NFToken": 
                                                {
                                                    "NFTokenID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF",
                                                    "URI": "7465737475726C"
                                                }
                                            },
                                            {
                                                "NFToken": 
                                                {
                                                    "NFTokenID": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                                                    "URI": "7465737475726C"
                                                }
                                            }
                                        ]
                                    },
                                    "LedgerEntryType": "NFTokenPage",
                                    "PreviousFields": 
                                    {
                                        "NFTokens": 
                                        [
                                            {
                                                "NFToken": 
                                                {
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
                        "nftoken_id": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF"
                    },
                    "tx": 
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee": "50",
                        "NFTokenTaxon": 123,
                        "Sequence": 1,
                        "SigningPubKey": "74657374",
                        "TransactionType": "NFTokenMint",
                        "hash": "C74463F49CFDCBEF3E9902672719918CDE5042DC7E7660BEBD1D1105C4B6DFF4",
                        "ledger_index": 11,
                        "inLedger": 11,
                        "date": 1
                    },
                    "validated": true
                },
                {
                    "meta": 
                    {
                        "AffectedNodes": 
                        [
                            {
                                "DeletedNode": 
                                {
                                    "FinalFields": 
                                    {
                                        "NFTokenID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA"
                                    },
                                    "LedgerEntryType": "NFTokenOffer"
                                }
                            }
                        ],
                        "TransactionIndex": 0,
                        "TransactionResult": "tesSUCCESS",
                        "nftoken_id": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA"
                    },
                    "tx": 
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee": "50",
                        "NFTokenBuyOffer": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                        "Sequence": 1,
                        "SigningPubKey": "74657374",
                        "TransactionType": "NFTokenAcceptOffer",
                        "hash": "7682BE6BCDE62F8142915DD852936623B68FC3839A8A424A6064B898702B0CDF",
                        "ledger_index": 11,
                        "inLedger": 11,
                        "date": 2
                    },
                    "validated": true
                },
                {
                    "meta": 
                    {
                        "AffectedNodes": 
                        [
                            {
                                "DeletedNode": {
                                    "FinalFields": 
                                    {
                                        "NFTokenID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA"
                                    },
                                    "LedgerEntryType": "NFTokenOffer"
                                }
                            },
                            {
                                "DeletedNode": 
                                {
                                    "FinalFields": 
                                    {
                                        "NFTokenID": "15FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF"
                                    },
                                    "LedgerEntryType": "NFTokenOffer"
                                }
                            }
                        ],
                        "TransactionIndex": 0,
                        "TransactionResult": "tesSUCCESS",
                        "nftoken_ids": 
                        [
                            "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA",
                            "15FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF"
                        ]
                    },
                    "tx": 
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee": "50",
                        "NFTokenOffers": 
                        [
                            "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA",
                            "15FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF"
                        ],
                        "Sequence": 1,
                        "SigningPubKey": "74657374",
                        "TransactionType": "NFTokenCancelOffer",
                        "hash": "9F82743EEB30065FB9CB92C61F0F064B5859C5A590FA811FAAAD9C988E5B47DB",
                        "ledger_index": 11,
                        "inLedger": 11,
                        "date": 3
                    },
                    "validated": true
                },
                {
                    "meta": 
                    {
                        "AffectedNodes": 
                        [
                            {
                                "CreatedNode": 
                                {
                                    "LedgerEntryType": "NFTokenOffer",
                                    "LedgerIndex": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA"
                                }
                            }
                        ],
                        "TransactionIndex": 0,
                        "TransactionResult": "tesSUCCESS",
                        "offer_id": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA"
                    },
                    "tx": 
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Amount": "123",
                        "Fee": "50",
                        "NFTokenID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF",
                        "Sequence": 1,
                        "SigningPubKey": "74657374",
                        "TransactionType": "NFTokenCreateOffer",
                        "hash": "ECB1837EB7C7C0AC22ECDCCE59FDD4795C70E0B9D8F4E1C9A9408BB7EC75DA5C",
                        "ledger_index": 11,
                        "inLedger": 11,
                        "date": 4
                    },
                    "validated": true
                }
            ],
            "validated": true,
            "marker": 
            {
                "ledger": 12,
                "seq": 34
            }
        })";

    auto const transactions = genNFTTransactions(kMIN_SEQ + 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_, testing::_, false, testing::Optional(testing::Eq(TransactionsCursor{10, 11})), testing::_
        )
    )
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false,
                "marker": {{"ledger": 10, "seq": 11}}
            }})",
            kACCOUNT,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{.yield = yield, .apiVersion = 1u});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(out));
    });
}

TEST_F(RPCAccountTxHandlerTest, NFTTxs_API_v2)
{
    auto const out = R"({
            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
            "ledger_index_min": 10,
            "ledger_index_max": 30,
            "transactions": [
                {
                    "meta": {
                        "AffectedNodes": 
                        [
                            {
                                "ModifiedNode": 
                                {
                                    "FinalFields": 
                                    {
                                        "NFTokens": 
                                        [
                                            {
                                                "NFToken": 
                                                {
                                                    "NFTokenID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF",
                                                    "URI": "7465737475726C"
                                                }
                                            },
                                            {
                                                "NFToken": 
                                                {
                                                    "NFTokenID": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                                                    "URI": "7465737475726C"
                                                }
                                            }
                                        ]
                                    },
                                    "LedgerEntryType": "NFTokenPage",
                                    "PreviousFields": 
                                    {
                                        "NFTokens": 
                                        [
                                            {
                                                "NFToken": 
                                                {
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
                        "nftoken_id": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF"
                    },
                    "hash": "C74463F49CFDCBEF3E9902672719918CDE5042DC7E7660BEBD1D1105C4B6DFF4",
                    "ledger_index": 11,
                    "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                    "close_time_iso": "2000-01-01T00:00:00Z",
                    "tx_json": 
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee": "50",
                        "NFTokenTaxon": 123,
                        "Sequence": 1,
                        "SigningPubKey": "74657374",
                        "TransactionType": "NFTokenMint",
                        "ledger_index": 11,
                        "date": 1
                    },
                    "validated": true
                },
                {
                    "meta": 
                    {
                        "AffectedNodes": 
                        [
                            {
                                "DeletedNode": 
                                {
                                    "FinalFields": 
                                    {
                                        "NFTokenID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA"
                                    },
                                    "LedgerEntryType": "NFTokenOffer"
                                }
                            }
                        ],
                        "TransactionIndex": 0,
                        "TransactionResult": "tesSUCCESS",
                        "nftoken_id": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA"
                    },
                    "hash": "7682BE6BCDE62F8142915DD852936623B68FC3839A8A424A6064B898702B0CDF",
                    "ledger_index": 11,
                    "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                    "close_time_iso": "2000-01-01T00:00:00Z",
                    "tx_json": 
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee": "50",
                        "NFTokenBuyOffer": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                        "Sequence": 1,
                        "SigningPubKey": "74657374",
                        "TransactionType": "NFTokenAcceptOffer",
                        "ledger_index": 11,
                        "date": 2
                    },
                    "validated": true
                },
                {
                    "meta": 
                    {
                        "AffectedNodes": 
                        [
                            {
                                "DeletedNode": {
                                    "FinalFields": 
                                    {
                                        "NFTokenID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA"
                                    },
                                    "LedgerEntryType": "NFTokenOffer"
                                }
                            },
                            {
                                "DeletedNode": 
                                {
                                    "FinalFields": 
                                    {
                                        "NFTokenID": "15FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF"
                                    },
                                    "LedgerEntryType": "NFTokenOffer"
                                }
                            }
                        ],
                        "TransactionIndex": 0,
                        "TransactionResult": "tesSUCCESS",
                        "nftoken_ids": 
                        [
                            "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA",
                            "15FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF"
                        ]
                    },
                    "hash": "9F82743EEB30065FB9CB92C61F0F064B5859C5A590FA811FAAAD9C988E5B47DB",
                    "ledger_index": 11,
                    "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                    "close_time_iso": "2000-01-01T00:00:00Z",
                    "tx_json": 
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee": "50",
                        "NFTokenOffers": 
                        [
                            "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA",
                            "15FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF"
                        ],
                        "Sequence": 1,
                        "SigningPubKey": "74657374",
                        "TransactionType": "NFTokenCancelOffer",
                        "ledger_index": 11,
                        "date": 3
                    },
                    "validated": true
                },
                {
                    "meta": 
                    {
                        "AffectedNodes": 
                        [
                            {
                                "CreatedNode": 
                                {
                                    "LedgerEntryType": "NFTokenOffer",
                                    "LedgerIndex": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA"
                                }
                            }
                        ],
                        "TransactionIndex": 0,
                        "TransactionResult": "tesSUCCESS",
                        "offer_id": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DA"
                    },
                    "hash": "ECB1837EB7C7C0AC22ECDCCE59FDD4795C70E0B9D8F4E1C9A9408BB7EC75DA5C",
                    "ledger_index": 11,
                    "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                    "close_time_iso": "2000-01-01T00:00:00Z",
                    "tx_json": 
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Amount": "123",
                        "Fee": "50",
                        "NFTokenID": "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DF",
                        "Sequence": 1,
                        "SigningPubKey": "74657374",
                        "TransactionType": "NFTokenCreateOffer",
                        "ledger_index": 11,
                        "date": 4
                    },
                    "validated": true
                }
            ],
            "validated": true,
            "marker": 
            {
                "ledger": 12,
                "seq": 34
            }
        })";

    auto const transactions = genNFTTransactions(kMIN_SEQ + 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_,
        fetchAccountTransactions(
            testing::_, testing::_, false, testing::Optional(testing::Eq(TransactionsCursor{10, 11})), testing::_
        )
    )
        .Times(1);

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, 11);
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(transactions.size()).WillRepeatedly(Return(ledgerHeader));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto static const kINPUT = json::parse(fmt::format(
            R"({{
                "account": "{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false,
                "marker": {{"ledger": 10, "seq": 11}}
            }})",
            kACCOUNT,
            -1,
            -1
        ));
        auto const output = handler.process(kINPUT, Context{.yield = yield, .apiVersion = 2u});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output.result, json::parse(out));
    });
}

struct AccountTxTransactionBundle {
    std::string testName;
    std::string testJson;
    std::string result;
    std::uint32_t apiVersion = 2u;
};

// parameterized test cases for parameters check
struct AccountTxTransactionTypeTest : public RPCAccountTxHandlerTest,
                                      public WithParamInterface<AccountTxTransactionBundle> {};

static auto
generateTransactionTypeTestValues()
{
    return std::vector<AccountTxTransactionBundle>{
        AccountTxTransactionBundle{
            .testName = "DIDSet",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "DIDSet"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "DIDDelete",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "DIDDelete"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "AccountSet",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "AccountSet"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "AccountDelete",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "AccountDelete"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "AMMBid",
            .testJson = R"({
                "account": "rLNaPoKeeBjZe2qs6x52yVPZpZ8td4dc6w",
                "ledger_index": "validated",
                "tx_type": "AMMBid"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "AMMCreate",
            .testJson = R"({
                "account": "rLNaPoKeeBjZe2qs6x52yVPZpZ8td4dc6w",
                "ledger_index": "validated",
                "tx_type": "AMMCreate"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "AMMDelete",
            .testJson = R"({
                "account": "rLNaPoKeeBjZe2qs6x52yVPZpZ8td4dc6w",
                "ledger_index": "validated",
                "tx_type": "AMMDelete"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "AMMDeposit",
            .testJson = R"({
                "account": "rLNaPoKeeBjZe2qs6x52yVPZpZ8td4dc6w",
                "ledger_index": "validated",
                "tx_type": "AMMDeposit"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "AMMVote",
            .testJson = R"({
                "account": "rLNaPoKeeBjZe2qs6x52yVPZpZ8td4dc6w",
                "ledger_index": "validated",
                "tx_type": "AMMVote"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "CheckCancel",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "CheckCancel"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "CheckCash",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "CheckCash"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "CheckCreate",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "CheckCreate"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "Clawback",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "Clawback"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "DepositPreauth",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "DepositPreauth"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "EscrowCancel",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "EscrowCancel"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "EscrowCreate",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "EscrowCreate"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "EscrowFinish",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "EscrowFinish"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "NFTokenAcceptOffer",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "NFTokenAcceptOffer"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "NFTokenBurn",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "NFTokenBurn"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "NFTokenCancelOffer",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "NFTokenCancelOffer"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "NFTokenCreateOffer",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "NFTokenCreateOffer"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "NFTokenMint",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "NFTokenMint"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "OfferCancel",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "OfferCancel"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "OfferCreate",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "OfferCreate"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "Payment_API_v1",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "Payment"
            })",
            .result = R"([
                {
                    "meta": {
                        "AffectedNodes": [
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "Balance": "22"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        },
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                    "Balance": "23"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        }],
                        "TransactionIndex": 0,
                        "TransactionResult": "tesSUCCESS",
                        "delivered_amount": "unavailable"
                    },
                    "tx": {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Amount": "1",
                        "DeliverMax": "1",
                        "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "Fee": "1",
                        "Sequence": 32,
                        "SigningPubKey": "74657374",
                        "TransactionType": "Payment",
                        "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                        "ledger_index": 30,
                        "inLedger": 30,
                        "date": 1
                    },
                    "validated": true
                }
            ])",
            .apiVersion = 1u
        },
        AccountTxTransactionBundle{
            .testName = "Lowercase_Payment",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "payment"
            })",
            .result = R"([
                {
                    "meta": {
                        "AffectedNodes": [
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "Balance": "22"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        },
                        {
                            "ModifiedNode": {
                                "FinalFields": {
                                    "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                    "Balance": "23"
                                },
                                "LedgerEntryType": "AccountRoot"
                            }
                        }],
                        "TransactionIndex": 0,
                        "TransactionResult": "tesSUCCESS",
                        "delivered_amount": "unavailable"
                    },
                    "tx": {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Amount": "1",
                        "DeliverMax": "1",
                        "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "Fee": "1",
                        "Sequence": 32,
                        "SigningPubKey": "74657374",
                        "TransactionType": "Payment",
                        "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                        "ledger_index": 30,
                        "inLedger": 30,
                        "date": 1
                    },
                    "validated": true
                }
            ])",
            .apiVersion = 1u
        },
        AccountTxTransactionBundle{
            .testName = "Payment_API_v2",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "Payment"
            })",
            .result = R"([
                {
                "hash": "51D2AAA6B8E4E16EF22F6424854283D8391B56875858A711B8CE4D5B9A422CC2",
                "ledger_index": 30,
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "close_time_iso": "2000-01-01T00:00:00Z",
                "meta": {
                    "AffectedNodes": [
                    {
                        "ModifiedNode": {
                            "FinalFields": {
                                "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                "Balance": "22"
                            },
                            "LedgerEntryType": "AccountRoot"
                        }
                    },
                    {
                        "ModifiedNode": {
                            "FinalFields": {
                                "Account": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                "Balance": "23"
                            },
                            "LedgerEntryType": "AccountRoot"
                        }
                    }],
                    "TransactionIndex": 0,
                    "TransactionResult": "tesSUCCESS",
                    "delivered_amount": "unavailable"
                },
                "tx_json": {
                    "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                    "DeliverMax": "1",
                    "Destination": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                    "Fee": "1",
                    "Sequence": 32,
                    "SigningPubKey": "74657374",
                    "TransactionType": "Payment",
                    "ledger_index": 30,
                    "date": 1
                },
                "validated": true
                }
            ])",
            .apiVersion = 2u
        },
        AccountTxTransactionBundle{
            .testName = "FilterWhenBinaryTrue",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "Payment",
                "binary": true
            })",
            .result = R"([{
                "meta": "201C00000000F8E5110061E762400000000000001681144B4E9C06F24296074F7BC48F92A97916C6DC5EA9E1E1E5110061E76240000000000000178114D31252CF902EF8DD8451243869B38667CBD89DF3E1E1F1031000",
                "tx_blob": "120000240000002061400000000000000168400000000000000173047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451243869B38667CBD89DF3",
                "ledger_index": 30,
                "validated": true
            }])",
            .apiVersion = 1u
        },
        AccountTxTransactionBundle{
            .testName = "PaymentChannelClaim",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "PaymentChannelClaim",
                "binary": true
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "FilterWhenBinaryTrueEmptyResult",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "PaymentChannelClaim"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "PaymentChannelCreate",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "PaymentChannelCreate"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "PaymentChannelFund",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "PaymentChannelFund"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "SetRegularKey",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "SetRegularKey"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "SignerListSet",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "SignerListSet"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "TicketCreate",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "TicketCreate"
            })",
            .result = "[]"
        },
        AccountTxTransactionBundle{
            .testName = "TrustSet",
            .testJson = R"({
                "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index": "validated",
                "tx_type": "TrustSet"
            })",
            .result = "[]"
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountTxTransactionTypeTest,
    AccountTxTransactionTypeTest,
    ValuesIn(generateTransactionTypeTestValues()),
    tests::util::kNAME_GENERATOR
);

TEST_P(AccountTxTransactionTypeTest, SpecificTransactionType)
{
    auto const transactions = genTransactions(kMAX_SEQ, kMAX_SEQ - 1);
    auto const transCursor = TransactionsAndCursor{.txns = transactions, .cursor = TransactionsCursor{12, 34}};
    ON_CALL(*backend_, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *backend_, fetchAccountTransactions(_, _, false, Optional(Eq(TransactionsCursor{kMAX_SEQ, INT32_MAX})), _)
    )
        .Times(1);

    auto const ledgerHeader = createLedgerHeader(kLEDGER_HASH, kMAX_SEQ);
    ON_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kMAX_SEQ, _)).Times(Between(1, 2));

    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{AccountTxHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{.yield = yield, .apiVersion = testBundle.apiVersion});
        EXPECT_TRUE(output);

        auto const transactions = output.result->at("transactions").as_array();
        auto const jsonObject = json::parse(testBundle.result);
        EXPECT_EQ(jsonObject, transactions);
    });
}
