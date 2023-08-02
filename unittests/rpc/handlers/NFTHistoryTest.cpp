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
#include <rpc/handlers/NFTHistory.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto MINSEQ = 10;
constexpr static auto MAXSEQ = 30;
constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto NFTID = "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004";

class RPCNFTHistoryHandlerTest : public HandlerBaseTest
{
};

struct NFTHistoryParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct NFTHistoryParameterTest : public RPCNFTHistoryHandlerTest,
                                 public WithParamInterface<NFTHistoryParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<NFTHistoryParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<NFTHistoryParamTestCaseBundle>{
        NFTHistoryParamTestCaseBundle{"MissingNFTID", R"({})", "invalidParams", "Required field 'nft_id' missing"},
        NFTHistoryParamTestCaseBundle{
            "BinaryNotBool",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "binary": 1})",
            "invalidParams",
            "Invalid parameters."},
        NFTHistoryParamTestCaseBundle{
            "ForwardNotBool",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "forward": 1})",
            "invalidParams",
            "Invalid parameters."},
        NFTHistoryParamTestCaseBundle{
            "ledger_index_minNotInt",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "ledger_index_min": "x"})",
            "invalidParams",
            "Invalid parameters."},
        NFTHistoryParamTestCaseBundle{
            "ledger_index_maxNotInt",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "ledger_index_max": "x"})",
            "invalidParams",
            "Invalid parameters."},
        NFTHistoryParamTestCaseBundle{
            "ledger_indexInvalid",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "ledger_index": "x"})",
            "invalidParams",
            "ledgerIndexMalformed"},
        NFTHistoryParamTestCaseBundle{
            "ledger_hashInvalid",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "ledger_hash": "x"})",
            "invalidParams",
            "ledger_hashMalformed"},
        NFTHistoryParamTestCaseBundle{
            "ledger_hashNotString",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "ledger_hash": 123})",
            "invalidParams",
            "ledger_hashNotString"},
        NFTHistoryParamTestCaseBundle{
            "limitNotInt",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "limit": "123"})",
            "invalidParams",
            "Invalid parameters."},
        NFTHistoryParamTestCaseBundle{
            "limitNagetive",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "limit": -1})",
            "invalidParams",
            "Invalid parameters."},
        NFTHistoryParamTestCaseBundle{
            "limitZero",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "limit": 0})",
            "invalidParams",
            "Invalid parameters."},
        NFTHistoryParamTestCaseBundle{
            "MarkerNotObject",
            R"({"nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", "marker": 101})",
            "invalidParams",
            "invalidMarker"},
        NFTHistoryParamTestCaseBundle{
            "MarkerMissingSeq",
            R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "marker": {"ledger": 123}
            })",
            "invalidParams",
            "Required field 'seq' missing"},
        NFTHistoryParamTestCaseBundle{
            "MarkerMissingLedger",
            R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "marker":{"seq": 123}
            })",
            "invalidParams",
            "Required field 'ledger' missing"},
        NFTHistoryParamTestCaseBundle{
            "MarkerLedgerNotInt",
            R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "marker": 
                {
                    "seq": "string",
                    "ledger": 1
                }
            })",
            "invalidParams",
            "Invalid parameters."},
        NFTHistoryParamTestCaseBundle{
            "MarkerSeqNotInt",
            R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "marker": 
                {
                    "ledger": "string",
                    "seq": 1
                }
            })",
            "invalidParams",
            "Invalid parameters."},
        NFTHistoryParamTestCaseBundle{
            "LedgerIndexMinLessThanMinSeq",
            R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "ledger_index_min": 9
            })",
            "lgrIdxMalformed",
            "ledgerSeqMinOutOfRange"},
        NFTHistoryParamTestCaseBundle{
            "LedgerIndexMaxLargeThanMaxSeq",
            R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "ledger_index_max": 31
            })",
            "lgrIdxMalformed",
            "ledgerSeqMaxOutOfRange"},
        NFTHistoryParamTestCaseBundle{
            "LedgerIndexMaxLessThanLedgerIndexMin",
            R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
                "ledger_index_max": 11,
                "ledger_index_min": 20
            })",
            "lgrIdxsInvalid",
            "Ledger indexes invalid."},
        NFTHistoryParamTestCaseBundle{
            "LedgerIndexMaxMinAndLedgerIndex",
            R"({
                "nft_id":"00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004", 
                "ledger_index_max": 20,
                "ledger_index_min": 11,
                "ledger_index": 10
            })",
            "invalidParams",
            "containsLedgerSpecifierAndRange"},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCNFTHistoryGroup1,
    NFTHistoryParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    NFTHistoryParameterTest::NameGenerator{});

TEST_P(NFTHistoryParameterTest, InvalidParams)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

static std::vector<TransactionAndMetadata>
genTransactions(uint32_t seq1, uint32_t seq2)
{
    auto transactions = std::vector<TransactionAndMetadata>{};
    auto trans1 = TransactionAndMetadata();
    ripple::STObject obj = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 1, 1, 32);
    trans1.transaction = obj.getSerializer().peekData();
    trans1.ledgerSequence = seq1;
    ripple::STObject metaObj = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 22, 23);
    trans1.metadata = metaObj.getSerializer().peekData();
    trans1.date = 1;
    transactions.push_back(trans1);

    auto trans2 = TransactionAndMetadata();
    ripple::STObject obj2 = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 1, 1, 32);
    trans2.transaction = obj.getSerializer().peekData();
    trans2.ledgerSequence = seq2;
    ripple::STObject metaObj2 = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 22, 23);
    trans2.metadata = metaObj2.getSerializer().peekData();
    trans2.date = 2;
    transactions.push_back(trans2);
    return transactions;
}

TEST_F(RPCNFTHistoryHandlerTest, IndexSpecificForwardTrue)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            true,
            testing::Optional(testing::Eq(TransactionsCursor{MINSEQ + 1, 0})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": true
            }})",
            NFTID,
            MINSEQ + 1,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ + 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output->as_object().contains("limit"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, IndexSpecificForwardFalse)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 1, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            NFTID,
            MINSEQ + 1,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ + 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output->as_object().contains("limit"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, IndexNotSpecificForwardTrue)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_, testing::_, true, testing::Optional(testing::Eq(TransactionsCursor{MINSEQ, 0})), testing::_))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": true
            }})",
            NFTID,
            -1,
            -1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output->as_object().contains("limit"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, IndexNotSpecificForwardFalse)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            NFTID,
            -1,
            -1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output->as_object().contains("limit"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, BinaryTrue)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "binary": true
            }})",
            NFTID,
            -1,
            -1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
        EXPECT_EQ(
            output->at("transactions").as_array()[0].as_object().at("meta").as_string(),
            "201C00000000F8E5110061E762400000000000001681144B4E9C06F24296074F7B"
            "C48F92A97916C6DC5EA9E1E1E5110061E76240000000000000178114D31252CF90"
            "2EF8DD8451243869B38667CBD89DF3E1E1F1031000");
        EXPECT_EQ(
            output->at("transactions").as_array()[0].as_object().at("tx_blob").as_string(),
            "120000240000002061400000000000000168400000000000000173047465737481"
            "144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451"
            "243869B38667CBD89DF3");
        EXPECT_EQ(output->at("transactions").as_array()[0].as_object().at("date").as_uint64(), 1);

        EXPECT_FALSE(output->as_object().contains("limit"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, LimitAndMarker)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_, testing::_, false, testing::Optional(testing::Eq(TransactionsCursor{10, 11})), testing::_))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "limit": 2,
                "forward": false,
                "marker": {{"ledger":10,"seq":11}}
            }})",
            NFTID,
            -1,
            -1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ);
        EXPECT_EQ(output->at("limit").as_uint64(), 2);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
    });
}

TEST_F(RPCNFTHistoryHandlerTest, SpecificLedgerIndex)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // adjust the order for forward->false
    auto const transactions = genTransactions(MAXSEQ - 1, MINSEQ + 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 1, INT32_MAX})),
            testing::_))
        .Times(1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ - 1);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(MAXSEQ - 1, _)).WillByDefault(Return(ledgerinfo));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index":{}
            }})",
            NFTID,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_FALSE(output->as_object().contains("limit"));
        EXPECT_FALSE(output->as_object().contains("marker"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCNFTHistoryHandlerTest, SpecificNonexistLedgerIntIndex)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(MAXSEQ - 1, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index":{}
            }})",
            NFTID,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNFTHistoryHandlerTest, SpecificNonexistLedgerStringIndex)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(MAXSEQ - 1, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index":"{}"
            }})",
            NFTID,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCNFTHistoryHandlerTest, SpecificLedgerHash)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // adjust the order for forward->false
    auto const transactions = genTransactions(MAXSEQ - 1, MINSEQ + 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 1, INT32_MAX})),
            testing::_))
        .Times(1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ - 1);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_hash":"{}"
            }})",
            NFTID,
            LEDGERHASH));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_FALSE(output->as_object().contains("limit"));
        EXPECT_FALSE(output->as_object().contains("marker"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCNFTHistoryHandlerTest, TxLessThanMinSeq)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MAXSEQ - 1, MINSEQ + 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 1, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            NFTID,
            MINSEQ + 2,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ + 2);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output->as_object().contains("limit"));
        EXPECT_FALSE(output->as_object().contains("marker"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, TxLargerThanMaxSeq)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MAXSEQ - 1, MINSEQ + 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 2, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            NFTID,
            MINSEQ + 1,
            MAXSEQ - 2));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ + 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 2);
        EXPECT_EQ(output->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output->as_object().contains("limit"));
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
    });
}

TEST_F(RPCNFTHistoryHandlerTest, LimitMoreThanMax)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchNFTTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchNFTTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 1, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTHistoryHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "nft_id":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false,
                "limit": {}
            }})",
            NFTID,
            MINSEQ + 1,
            MAXSEQ - 1,
            NFTHistoryHandler::LIMIT_MAX + 1));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("nft_id").as_string(), NFTID);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ + 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
        EXPECT_EQ(output->as_object().at("limit").as_uint64(), NFTHistoryHandler::LIMIT_MAX);
    });
}
