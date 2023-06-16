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
#include <rpc/handlers/AccountTx.h>
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

class RPCAccountTxHandlerTest : public HandlerBaseTest
{
};

struct AccountTxParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AccountTxParameterTest : public RPCAccountTxHandlerTest, public WithParamInterface<AccountTxParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<AccountTxParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AccountTxParamTestCaseBundle>{
        AccountTxParamTestCaseBundle{"MissingAccount", R"({})", "invalidParams", "Required field 'account' missing"},
        AccountTxParamTestCaseBundle{
            "BinaryNotBool",
            R"({"account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "binary": 1})",
            "invalidParams",
            "Invalid parameters."},
        AccountTxParamTestCaseBundle{
            "ForwardNotBool",
            R"({"account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "forward": 1})",
            "invalidParams",
            "Invalid parameters."},
        AccountTxParamTestCaseBundle{
            "ledger_index_minNotInt",
            R"({"account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index_min": "x"})",
            "invalidParams",
            "Invalid parameters."},
        AccountTxParamTestCaseBundle{
            "ledger_index_maxNotInt",
            R"({"account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index_max": "x"})",
            "invalidParams",
            "Invalid parameters."},
        AccountTxParamTestCaseBundle{
            "ledger_indexInvalid",
            R"({"account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": "x"})",
            "invalidParams",
            "ledgerIndexMalformed"},
        AccountTxParamTestCaseBundle{
            "ledger_hashInvalid",
            R"({"account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": "x"})",
            "invalidParams",
            "ledger_hashMalformed"},
        AccountTxParamTestCaseBundle{
            "ledger_hashNotString",
            R"({"account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": 123})",
            "invalidParams",
            "ledger_hashNotString"},
        AccountTxParamTestCaseBundle{
            "limitNotInt",
            R"({"account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": "123"})",
            "invalidParams",
            "Invalid parameters."},
        AccountTxParamTestCaseBundle{
            "limitOverRange",
            R"({"account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "limit": 101})",
            "invalidParams",
            "Invalid parameters."},
        AccountTxParamTestCaseBundle{
            "MarkerNotObject",
            R"({"account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "marker": 101})",
            "invalidParams",
            "invalidMarker"},
        AccountTxParamTestCaseBundle{
            "MarkerMissingSeq",
            R"({
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "marker": {"ledger": 123}
            })",
            "invalidParams",
            "Required field 'seq' missing"},
        AccountTxParamTestCaseBundle{
            "MarkerMissingLedger",
            R"({
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "marker":{"seq": 123}
            })",
            "invalidParams",
            "Required field 'ledger' missing"},
        AccountTxParamTestCaseBundle{
            "MarkerLedgerNotInt",
            R"({
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "marker": 
                {
                    "seq": "string",
                    "ledger": 1
                }
            })",
            "invalidParams",
            "Invalid parameters."},
        AccountTxParamTestCaseBundle{
            "MarkerSeqNotInt",
            R"({
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "marker": 
                {
                    "ledger": "string",
                    "seq": 1
                }
            })",
            "invalidParams",
            "Invalid parameters."},
        AccountTxParamTestCaseBundle{
            "LedgerIndexMinLessThanMinSeq",
            R"({
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_min": 9
            })",
            "lgrIdxMalformed",
            "ledgerSeqMinOutOfRange"},
        AccountTxParamTestCaseBundle{
            "LedgerIndexMaxLargeThanMaxSeq",
            R"({
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 31
            })",
            "lgrIdxMalformed",
            "ledgerSeqMaxOutOfRange"},
        AccountTxParamTestCaseBundle{
            "LedgerIndexMaxLessThanLedgerIndexMin",
            R"({
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "ledger_index_max": 11,
                "ledger_index_min": 20
            })",
            "lgrIdxsInvalid",
            "Ledger indexes invalid."},
        AccountTxParamTestCaseBundle{
            "LedgerIndexMaxMinAndLedgerIndex",
            R"({
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_index_max": 20,
                "ledger_index_min": 11,
                "ledger_index": 10
            })",
            "invalidParams",
            "containsLedgerSpecifierAndRange"},
        AccountTxParamTestCaseBundle{
            "LedgerIndexMaxMinAndLedgerIndexValidated",
            R"({
                "account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", 
                "ledger_index_max": 20,
                "ledger_index_min": 11,
                "ledger_index": "validated"
            })",
            "invalidParams",
            "containsLedgerSpecifierAndRange"},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountTxGroup1,
    AccountTxParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    AccountTxParameterTest::NameGenerator{});

TEST_P(AccountTxParameterTest, InvalidParams)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    auto const testBundle = GetParam();
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{std::ref(yield)});
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

TEST_F(RPCAccountTxHandlerTest, IndexSpecificForwardTrue)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            true,
            testing::Optional(testing::Eq(TransactionsCursor{MINSEQ, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": true
            }})",
            ACCOUNT,
            MINSEQ + 1,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ + 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output->as_object().contains("limit"));
    });
}

TEST_F(RPCAccountTxHandlerTest, IndexSpecificForwardFalse)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 1, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            ACCOUNT,
            MINSEQ + 1,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ + 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output->as_object().contains("limit"));
    });
}

TEST_F(RPCAccountTxHandlerTest, IndexNotSpecificForwardTrue)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_, testing::_, true, testing::Optional(testing::Eq(TransactionsCursor{MINSEQ-1, INT32_MAX})), testing::_))
        .Times(1);

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": true
            }})",
            ACCOUNT,
            -1,
            -1));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output->as_object().contains("limit"));
    });
}

TEST_F(RPCAccountTxHandlerTest, IndexNotSpecificForwardFalse)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            ACCOUNT,
            -1,
            -1));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
        EXPECT_FALSE(output->as_object().contains("limit"));
    });
}

TEST_F(RPCAccountTxHandlerTest, BinaryTrue)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "binary": true
            }})",
            ACCOUNT,
            -1,
            -1));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
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

TEST_F(RPCAccountTxHandlerTest, LimitAndMarker)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MINSEQ + 1, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_, testing::_, false, testing::Optional(testing::Eq(TransactionsCursor{10, 11})), testing::_))
        .Times(1);

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "limit": 2,
                "forward": false,
                "marker": {{"ledger":10,"seq":11}}
            }})",
            ACCOUNT,
            -1,
            -1));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ);
        EXPECT_EQ(output->at("limit").as_uint64(), 2);
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 2);
    });
}

TEST_F(RPCAccountTxHandlerTest, SpecificLedgerIndex)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // adjust the order for forward->false
    auto const transactions = genTransactions(MAXSEQ - 1, MINSEQ + 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 1, INT32_MAX})),
            testing::_))
        .Times(1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ - 1);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(MAXSEQ - 1, _)).WillByDefault(Return(ledgerinfo));

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index":{}
            }})",
            ACCOUNT,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_FALSE(output->as_object().contains("limit"));
        EXPECT_FALSE(output->as_object().contains("marker"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCAccountTxHandlerTest, SpecificNonexistLedgerIntIndex)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(MAXSEQ - 1, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index":{}
            }})",
            ACCOUNT,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountTxHandlerTest, SpecificNonexistLedgerStringIndex)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(MAXSEQ - 1, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index":"{}"
            }})",
            ACCOUNT,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountTxHandlerTest, SpecificLedgerHash)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // adjust the order for forward->false
    auto const transactions = genTransactions(MAXSEQ - 1, MINSEQ + 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 1, INT32_MAX})),
            testing::_))
        .Times(1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ - 1);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(ledgerinfo));

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_hash":"{}"
            }})",
            ACCOUNT,
            LEDGERHASH));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_FALSE(output->as_object().contains("limit"));
        EXPECT_FALSE(output->as_object().contains("marker"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCAccountTxHandlerTest, SpecificLedgerIndexValidated)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    // adjust the order for forward->false
    auto const transactions = genTransactions(MAXSEQ, MAXSEQ - 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ, INT32_MAX})),
            testing::_))
        .Times(1);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(MAXSEQ, _)).WillByDefault(Return(ledgerinfo));

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index":"validated"
            }})",
            ACCOUNT));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MAXSEQ);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ);
        EXPECT_FALSE(output->as_object().contains("limit"));
        EXPECT_FALSE(output->as_object().contains("marker"));
        EXPECT_EQ(output->at("transactions").as_array().size(), 1);
    });
}

TEST_F(RPCAccountTxHandlerTest, TxLessThanMinSeq)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MAXSEQ - 1, MINSEQ + 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 1, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            ACCOUNT,
            MINSEQ + 2,
            MAXSEQ - 1));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ + 2);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 1);
        EXPECT_EQ(output->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output->as_object().contains("limit"));
        EXPECT_FALSE(output->as_object().contains("marker"));
    });
}

TEST_F(RPCAccountTxHandlerTest, TxLargerThanMaxSeq)
{
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    MockBackend* rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const transactions = genTransactions(MAXSEQ - 1, MINSEQ + 1);
    auto const transCursor = TransactionsAndCursor{transactions, TransactionsCursor{12, 34}};
    ON_CALL(*rawBackendPtr, fetchAccountTransactions).WillByDefault(Return(transCursor));
    EXPECT_CALL(
        *rawBackendPtr,
        fetchAccountTransactions(
            testing::_,
            testing::_,
            false,
            testing::Optional(testing::Eq(TransactionsCursor{MAXSEQ - 2, INT32_MAX})),
            testing::_))
        .Times(1);

    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountTxHandler{mockBackendPtr}};
        auto const static input = boost::json::parse(fmt::format(
            R"({{
                "account":"{}",
                "ledger_index_min": {},
                "ledger_index_max": {},
                "forward": false
            }})",
            ACCOUNT,
            MINSEQ + 1,
            MAXSEQ - 2));
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("account").as_string(), ACCOUNT);
        EXPECT_EQ(output->at("ledger_index_min").as_uint64(), MINSEQ + 1);
        EXPECT_EQ(output->at("ledger_index_max").as_uint64(), MAXSEQ - 2);
        EXPECT_EQ(output->at("transactions").as_array().size(), 1);
        EXPECT_FALSE(output->as_object().contains("limit"));
        EXPECT_EQ(output->at("marker").as_object(), json::parse(R"({"ledger":12,"seq":34})"));
    });
}
