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
#include <rpc/handlers/Ledger.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto INDEX2 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B1";
constexpr static auto CURRENCY = "0158415500000000C1F76FF6ECB0BAC600000000";

constexpr static auto RANGEMIN = 10;
constexpr static auto RANGEMAX = 30;

using namespace RPC;
namespace json = boost::json;
using namespace testing;

class RPCLedgerHandlerTest : public HandlerBaseTest
{
};

struct LedgerParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct LedgerParameterTest : public RPCLedgerHandlerTest, public WithParamInterface<LedgerParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<LedgerParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<LedgerParamTestCaseBundle>{
        {
            "AccountsNotBool",
            R"({"accounts": 123})",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "AccountsInvalid",
            R"({"accounts": true})",
            "notSupported",
            "Not supported field 'accounts's value 'true'",
        },
        {
            "FullExist",
            R"({"full": true})",
            "notSupported",
            "Not supported field 'full's value 'true'",
        },
        {
            "FullNotBool",
            R"({"full": 123})",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "QueueExist",
            R"({"queue": true})",
            "notSupported",
            "Not supported field 'queue's value 'true'",
        },
        {
            "QueueNotBool",
            R"({"queue": 123})",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "OwnerFundsNotBool",
            R"({"owner_funds": 123})",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "LedgerHashInvalid",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": "x"})",
            "invalidParams",
            "ledger_hashMalformed",
        },
        {
            "LedgerHashNotString",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_hash": 123})",
            "invalidParams",
            "ledger_hashNotString",
        },
        {
            "LedgerIndexNotInt",
            R"({"account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn", "ledger_index": "x"})",
            "invalidParams",
            "ledgerIndexMalformed",
        },
        {
            "TransactionsNotBool",
            R"({"transactions": "x"})",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "ExpandNotBool",
            R"({"expand": "x"})",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "BinaryNotBool",
            R"({"binary": "x"})",
            "invalidParams",
            "Invalid parameters.",
        },
        {
            "DiffNotBool",
            R"({"diff": "x"})",
            "invalidParams",
            "Invalid parameters.",
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCLedgerGroup1,
    LedgerParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    LedgerParameterTest::NameGenerator{});

TEST_P(LedgerParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCLedgerHandlerTest, LedgerNotExistViaIntSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "ledger_index": {}
            }})",
            RANGEMAX));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerHandlerTest, LedgerNotExistViaStringSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "ledger_index": "{}"
            }})",
            RANGEMAX));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerHandlerTest, LedgerNotExistViaHash)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);  // min
    mockBackendPtr->updateRange(RANGEMAX);  // max

    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _)).WillByDefault(Return(std::nullopt));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(
            R"({{
                "ledger_hash": "{}"
            }})",
            LEDGERHASH));
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCLedgerHandlerTest, Default)
{
    static auto constexpr expectedOut =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "validated":true,
            "ledger":{
                "accepted":true,
                "account_hash":"0000000000000000000000000000000000000000000000000000000000000000",
                "close_flags":0,
                "close_time":0,
                "close_time_resolution":0,
                "closed":true,
                "hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index":"30",
                "parent_close_time":0,
                "parent_hash":"0000000000000000000000000000000000000000000000000000000000000000",
                "total_coins":"0",
                "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000"
            }
        })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse("{}");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        // remove human readable time, it is sightly different cross the platform
        EXPECT_EQ(output->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

// not supported fields can be set to its default value
TEST_F(RPCLedgerHandlerTest, NotSupportedFieldsDefaultValue)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "full": false,
                "accounts": false,
                "queue": false
            })");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
    });
}

TEST_F(RPCLedgerHandlerTest, QueryViaLedgerIndex)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(15, _)).WillByDefault(Return(ledgerinfo));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(R"({"ledger_index": 15})");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().contains("ledger"));
    });
}

TEST_F(RPCLedgerHandlerTest, QueryViaLedgerHash)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{INDEX1}, _)).WillByDefault(Return(ledgerinfo));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(fmt::format(R"({{"ledger_hash": "{}" }})", INDEX1));
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().contains("ledger"));
    });
}

TEST_F(RPCLedgerHandlerTest, BinaryTrue)
{
    static auto constexpr expectedOut =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "validated":true,
            "ledger":{
                "ledger_data":"0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "closed":true
            }
        })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "binary": true
            })");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, TransactionsExpandBinary)
{
    static auto constexpr expectedOut =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "validated":true,
            "ledger":{
                "ledger_data":"0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "closed":true,
                "transactions":[
                    {
                        "tx_blob":"120000240000001E61400000000000006468400000000000000373047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451243869B38667CBD89DF3",
                        "meta":"201C00000000F8E5110061E762400000000000006E81144B4E9C06F24296074F7BC48F92A97916C6DC5EA9E1E1E5110061E762400000000000001E8114D31252CF902EF8DD8451243869B38667CBD89DF3E1E1F1031000"
                    },
                    {
                        "tx_blob":"120000240000001E61400000000000006468400000000000000373047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA98314D31252CF902EF8DD8451243869B38667CBD89DF3",
                        "meta":"201C00000000F8E5110061E762400000000000006E81144B4E9C06F24296074F7BC48F92A97916C6DC5EA9E1E1E5110061E762400000000000001E8114D31252CF902EF8DD8451243869B38667CBD89DF3E1E1F1031000"
                    }
                ]
            }
        })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    TransactionAndMetadata t1;
    t1.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 100, 3, RANGEMAX).getSerializer().peekData();
    t1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = RANGEMAX;

    EXPECT_CALL(*rawBackendPtr, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*rawBackendPtr, fetchAllTransactionsInLedger(RANGEMAX, _)).WillByDefault(Return(std::vector{t1, t1}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "binary": true,
                "expand": true,
                "transactions": true
            })");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, TransactionsExpandNotBinary)
{
    static auto constexpr expectedOut =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "validated":true,
            "ledger":{
                "accepted":true,
                "account_hash":"0000000000000000000000000000000000000000000000000000000000000000",
                "close_flags":0,
                "close_time":0,
                "close_time_resolution":0,
                "closed":true,
                "hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index":"30",
                "parent_close_time":0,
                "parent_hash":"0000000000000000000000000000000000000000000000000000000000000000",
                "total_coins":"0",
                "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000",
                "transactions":[
                    {
                        "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Amount":"100",
                        "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "Fee":"3",
                        "Sequence":30,
                        "SigningPubKey":"74657374",
                        "TransactionType":"Payment",
                        "hash":"70436A9332F7CD928FAEC1A41269A677739D8B11F108CE23AE23CBF0C9113F8C",
                        "metaData":{
                        "AffectedNodes":[
                            {
                                "ModifiedNode":{
                                    "FinalFields":{
                                    "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "Balance":"110"
                                    },
                                    "LedgerEntryType":"AccountRoot"
                                }
                            },
                            {
                                "ModifiedNode":{
                                    "FinalFields":{
                                    "Account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                    "Balance":"30"
                                    },
                                    "LedgerEntryType":"AccountRoot"
                                }
                            }
                        ],
                        "TransactionIndex":0,
                        "TransactionResult":"tesSUCCESS",
                        "delivered_amount":"unavailable"
                        }
                    }
                ]
            }
        })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    TransactionAndMetadata t1;
    t1.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 100, 3, RANGEMAX).getSerializer().peekData();
    t1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = RANGEMAX;

    EXPECT_CALL(*rawBackendPtr, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*rawBackendPtr, fetchAllTransactionsInLedger(RANGEMAX, _)).WillByDefault(Return(std::vector{t1}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "binary": false,
                "expand": true,
                "transactions": true
            })");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        // remove human readable time, it is sightly different cross the platform
        EXPECT_EQ(output->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, TransactionsNotExpand)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    EXPECT_CALL(*rawBackendPtr, fetchAllTransactionHashesInLedger).Times(1);
    ON_CALL(*rawBackendPtr, fetchAllTransactionHashesInLedger(RANGEMAX, _))
        .WillByDefault(Return(std::vector{ripple::uint256{INDEX1}, ripple::uint256{INDEX2}}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "transactions": true
            })");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output->as_object().at("ledger").at("transactions"),
            json::parse(fmt::format(R"(["{}","{}"])", INDEX1, INDEX2)));
    });
}

TEST_F(RPCLedgerHandlerTest, DiffNotBinary)
{
    static auto constexpr expectedOut =
        R"([
            {
                "object_id":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B1",
                "object":""
            },
            {
                "object_id":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                "object":{
                "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "Balance":"10",
                "Flags":4194304,
                "LedgerEntryType":"AccountRoot",
                "OwnerCount":2,
                "PreviousTxnID":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                "PreviousTxnLgrSeq":3,
                "Sequence":1,
                "TransferRate":0,
                "index":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC"
                }
            }
        ])";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    std::vector<LedgerObject> los;

    EXPECT_CALL(*rawBackendPtr, fetchLedgerDiff).Times(1);

    los.push_back(LedgerObject{ripple::uint256{INDEX2}, Blob{}});
    los.push_back(LedgerObject{
        ripple::uint256{INDEX1},
        CreateAccountRootObject(ACCOUNT, ripple::lsfGlobalFreeze, 1, 10, 2, INDEX1, 3).getSerializer().peekData()});

    ON_CALL(*rawBackendPtr, fetchLedgerDiff(RANGEMAX, _)).WillByDefault(Return(los));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "diff": true
            })");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("ledger").at("diff"), json::parse(expectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, DiffBinary)
{
    static auto constexpr expectedOut =
        R"([
            {
                "object_id":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515B1",
                "object":""
            },
            {
                "object_id":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
                "object":"1100612200400000240000000125000000032B000000002D00000002551B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC62400000000000000A81144B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
            }
        ])";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    std::vector<LedgerObject> los;

    EXPECT_CALL(*rawBackendPtr, fetchLedgerDiff).Times(1);

    los.push_back(LedgerObject{ripple::uint256{INDEX2}, Blob{}});
    los.push_back(LedgerObject{
        ripple::uint256{INDEX1},
        CreateAccountRootObject(ACCOUNT, ripple::lsfGlobalFreeze, 1, 10, 2, INDEX1, 3).getSerializer().peekData()});

    ON_CALL(*rawBackendPtr, fetchLedgerDiff(RANGEMAX, _)).WillByDefault(Return(los));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "diff": true,
                "binary": true
            })");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->at("ledger").at("diff"), json::parse(expectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsEmtpy)
{
    static auto constexpr expectedOut =
        R"({
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index":30,
            "validated":true,
            "ledger":{
                "accepted":true,
                "account_hash":"0000000000000000000000000000000000000000000000000000000000000000",
                "close_flags":0,
                "close_time":0,
                "close_time_resolution":0,
                "closed":true,
                "hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index":"30",
                "parent_close_time":0,
                "parent_hash":"0000000000000000000000000000000000000000000000000000000000000000",
                "total_coins":"0",
                "transaction_hash":"0000000000000000000000000000000000000000000000000000000000000000",
                "transactions":[
                    {
                        "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Amount":"100",
                        "Destination":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                        "Fee":"3",
                        "Sequence":30,
                        "SigningPubKey":"74657374",
                        "TransactionType":"Payment",
                        "hash":"70436A9332F7CD928FAEC1A41269A677739D8B11F108CE23AE23CBF0C9113F8C",
                        "metaData":{
                        "AffectedNodes":[
                            {
                                "ModifiedNode":{
                                    "FinalFields":{
                                    "Account":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                    "Balance":"110"
                                    },
                                    "LedgerEntryType":"AccountRoot"
                                }
                            },
                            {
                                "ModifiedNode":{
                                    "FinalFields":{
                                    "Account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                    "Balance":"30"
                                    },
                                    "LedgerEntryType":"AccountRoot"
                                }
                            }
                        ],
                        "TransactionIndex":0,
                        "TransactionResult":"tesSUCCESS",
                        "delivered_amount":"unavailable"
                        }
                    }
                ]
            }
        })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    TransactionAndMetadata t1;
    t1.transaction = CreatePaymentTransactionObject(ACCOUNT, ACCOUNT2, 100, 3, RANGEMAX).getSerializer().peekData();
    t1.metadata = CreatePaymentTransactionMetaObject(ACCOUNT, ACCOUNT2, 110, 30).getSerializer().peekData();
    t1.ledgerSequence = RANGEMAX;

    EXPECT_CALL(*rawBackendPtr, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*rawBackendPtr, fetchAllTransactionsInLedger(RANGEMAX, _)).WillByDefault(Return(std::vector{t1}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "binary": false,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        // remove human readable time, it is sightly different cross the platform
        EXPECT_EQ(output->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsTrueBinaryFalse)
{
    static auto constexpr expectedOut =
        R"({
            "ledger": {
                "accepted": true,
                "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "close_flags": 0,
                "close_time": 0,
                "close_time_resolution": 0,
                "closed": true,
                "hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                "ledger_index": "30",
                "parent_close_time": 0,
                "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "total_coins": "0",
                "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
                "transactions": [
                    {
                        "Account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                        "Fee": "2",
                        "hash": "65757B01CC1DF860DC6FEC73D6435D902BDC5E52D3FCB519E83D91C1F3D82EDC",
                        "metaData": {
                            "AffectedNodes": [
                                {
                                    "CreatedNode": {
                                        "LedgerEntryType": "Offer",
                                        "NewFields": {
                                            "TakerGets": "300",
                                            "TakerPays": {
                                                "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                                                "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                "value": "200"
                                            }
                                        }
                                    }
                                }
                            ],
                            "TransactionIndex": 100,
                            "TransactionResult": "tesSUCCESS"
                        },
                        "owner_funds": "193",
                        "Sequence": 100,
                        "SigningPubKey": "74657374",
                        "TakerGets": "300",
                        "TakerPays": {
                            "currency": "0158415500000000C1F76FF6ECB0BAC600000000",
                            "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                            "value": "200"
                        },
                        "TransactionType": "OfferCreate"
                    }
                ]
            },
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true
        })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    // account doFetchLedgerObject
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    auto const accountObject =
        CreateAccountRootObject(ACCOUNT, 0, RANGEMAX, 200 /*balance*/, 2 /*owner object*/, INDEX1, RANGEMAX - 1, 0)
            .getSerializer()
            .peekData();
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, RANGEMAX, _)).WillByDefault(Return(accountObject));

    // fee object 2*2+3->7 ; balance 200 - 7 -> 193
    auto feeBlob = CreateFeeSettingBlob(1, 2 /*reserve inc*/, 3 /*reserve base*/, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::fees().key, RANGEMAX, _))
        .WillByDefault(Return(feeBlob));

    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT2, 100, 300, 200).getSerializer().peekData();
    tx.transaction = CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300, true)
                         .getSerializer()
                         .peekData();
    tx.date = 123456;
    tx.ledgerSequence = RANGEMAX;

    EXPECT_CALL(*rawBackendPtr, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*rawBackendPtr, fetchAllTransactionsInLedger(RANGEMAX, _)).WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "binary": false,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        // remove human readable time, it is sightly different cross the platform
        EXPECT_EQ(output->as_object().at("ledger").as_object().erase("close_time_human"), 1);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsTrueBinaryTrue)
{
    static auto constexpr expectedOut =
        R"({
            "ledger": {
                "closed": true,
                "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "transactions": [
                    {
                        "meta": "201C00000064F8E311006FE864D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF365400000000000012CE1E1F1031000",
                        "owner_funds": "193",
                        "tx_blob": "120007240000006464D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF365400000000000012C68400000000000000273047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
                    }
                ]
            },
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true
        })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    // account doFetchLedgerObject
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    auto const accountObject =
        CreateAccountRootObject(ACCOUNT, 0, RANGEMAX, 200 /*balance*/, 2 /*owner object*/, INDEX1, RANGEMAX - 1, 0)
            .getSerializer()
            .peekData();
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, RANGEMAX, _)).WillByDefault(Return(accountObject));

    // fee object 2*2+3->7 ; balance 200 - 7 -> 193
    auto feeBlob = CreateFeeSettingBlob(1, 2 /*reserve inc*/, 3 /*reserve base*/, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::fees().key, RANGEMAX, _))
        .WillByDefault(Return(feeBlob));

    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT2, 100, 300, 200).getSerializer().peekData();
    tx.transaction = CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300, true)
                         .getSerializer()
                         .peekData();
    tx.date = 123456;
    tx.ledgerSequence = RANGEMAX;

    EXPECT_CALL(*rawBackendPtr, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*rawBackendPtr, fetchAllTransactionsInLedger(RANGEMAX, _)).WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "binary": true,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsIssuerIsSelf)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    // issuer is self
    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT, 100, 300, 200).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = RANGEMAX;

    EXPECT_CALL(*rawBackendPtr, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*rawBackendPtr, fetchAllTransactionsInLedger(RANGEMAX, _)).WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "binary": true,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_FALSE(output->as_object()["ledger"].as_object()["transactions"].as_array()[0].as_object().contains(
            "owner_funds"));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsNotEnoughForReserve)
{
    static auto constexpr expectedOut =
        R"({
            "ledger": {
                "closed": true,
                "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                "transactions": [
                    {
                        "meta": "201C00000064F8E311006FE864D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF365400000000000012CE1E1F1031000",
                        "owner_funds": "0",
                        "tx_blob": "120007240000006464D5071AFD498D00000158415500000000C1F76FF6ECB0BAC600000000D31252CF902EF8DD8451243869B38667CBD89DF365400000000000012C68400000000000000273047465737481144B4E9C06F24296074F7BC48F92A97916C6DC5EA9"
                    }
                ]
            },
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_index": 30,
            "validated": true
        })";
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    // account doFetchLedgerObject
    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    auto const accountObject =
        CreateAccountRootObject(ACCOUNT, 0, RANGEMAX, 6 /*balance*/, 2 /*owner object*/, INDEX1, RANGEMAX - 1, 0)
            .getSerializer()
            .peekData();
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, RANGEMAX, _)).WillByDefault(Return(accountObject));

    // fee object 2*2+3->7 ; balance 6 - 7 -> -1
    auto feeBlob = CreateFeeSettingBlob(1, 2 /*reserve inc*/, 3 /*reserve base*/, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::fees().key, RANGEMAX, _))
        .WillByDefault(Return(feeBlob));

    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT2, 100, 300, 200).getSerializer().peekData();
    tx.transaction = CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300, true)
                         .getSerializer()
                         .peekData();
    tx.date = 123456;
    tx.ledgerSequence = RANGEMAX;

    EXPECT_CALL(*rawBackendPtr, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*rawBackendPtr, fetchAllTransactionsInLedger(RANGEMAX, _)).WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "binary": true,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsNotXRP)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    // mock line
    auto const line = CreateRippleStateLedgerObject(
        ACCOUNT, CURRENCY, ACCOUNT2, 50 /*balance*/, ACCOUNT, 10, ACCOUNT2, 20, INDEX1, 123);
    auto lineKey = ripple::keylet::line(
                       GetAccountIDWithString(ACCOUNT),
                       GetAccountIDWithString(ACCOUNT2),
                       ripple::to_currency(std::string(CURRENCY)))
                       .key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(lineKey, RANGEMAX, _))
        .WillByDefault(Return(line.getSerializer().peekData()));

    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT2, 100, 300, 200, true).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = RANGEMAX;

    EXPECT_CALL(*rawBackendPtr, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*rawBackendPtr, fetchAllTransactionsInLedger(RANGEMAX, _)).WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "binary": true,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output->as_object()["ledger"]
                .as_object()["transactions"]
                .as_array()[0]
                .as_object()["owner_funds"]
                .as_string(),
            "50");
    });
}

TEST_F(RPCLedgerHandlerTest, OwnerFundsIgnoreFreezeLine)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(RANGEMIN);
    mockBackendPtr->updateRange(RANGEMAX);

    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, RANGEMAX);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(RANGEMAX, _)).WillByDefault(Return(ledgerinfo));

    // mock line freeze
    auto const line = CreateRippleStateLedgerObject(
        ACCOUNT,
        CURRENCY,
        ACCOUNT2,
        50 /*balance*/,
        ACCOUNT,
        10,
        ACCOUNT2,
        20,
        INDEX1,
        123,
        ripple::lsfLowFreeze | ripple::lsfHighFreeze);
    auto lineKey = ripple::keylet::line(
                       GetAccountIDWithString(ACCOUNT),
                       GetAccountIDWithString(ACCOUNT2),
                       ripple::to_currency(std::string(CURRENCY)))
                       .key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(lineKey, RANGEMAX, _))
        .WillByDefault(Return(line.getSerializer().peekData()));

    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    TransactionAndMetadata tx;
    tx.metadata = CreateMetaDataForCreateOffer(CURRENCY, ACCOUNT2, 100, 300, 200, true).getSerializer().peekData();
    tx.transaction =
        CreateCreateOfferTransactionObject(ACCOUNT, 2, 100, CURRENCY, ACCOUNT2, 200, 300).getSerializer().peekData();
    tx.date = 123456;
    tx.ledgerSequence = RANGEMAX;

    EXPECT_CALL(*rawBackendPtr, fetchAllTransactionsInLedger).Times(1);
    ON_CALL(*rawBackendPtr, fetchAllTransactionsInLedger(RANGEMAX, _)).WillByDefault(Return(std::vector{tx}));

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{LedgerHandler{mockBackendPtr}};
        auto const req = json::parse(
            R"({
                "binary": true,
                "expand": true,
                "transactions": true,
                "owner_funds": true
            })");
        auto output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(
            output->as_object()["ledger"]
                .as_object()["transactions"]
                .as_array()[0]
                .as_object()["owner_funds"]
                .as_string(),
            "50");
    });
}
