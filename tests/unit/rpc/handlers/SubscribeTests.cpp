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
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/Subscribe.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/MockWsBase.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/UintTypes.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;
using std::chrono::milliseconds;

constexpr static auto MINSEQ = 10;
constexpr static auto MAXSEQ = 30;
constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto PAYS20USDGETS10XRPBOOKDIR = "43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000";
constexpr static auto PAYS20XRPGETS10USDBOOKDIR = "7B1767D41DBCE79D9585CF9D0262A5FEC45E5206FF524F8B55071AFD498D0000";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto INDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

struct RPCSubscribeHandlerTest : HandlerBaseTest {
    web::SubscriptionContextPtr session_ = std::make_shared<MockSession>();
    MockSession* mockSession_ = dynamic_cast<MockSession*>(session_.get());
    StrictMockSubscriptionManagerSharedPtr mockSubscriptionManagerPtr;
};

struct SubscribeParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct SubscribeParameterTest : public RPCSubscribeHandlerTest,
                                public WithParamInterface<SubscribeParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<SubscribeParamTestCaseBundle>{
        SubscribeParamTestCaseBundle{
            .testName = "AccountsNotArray",
            .testJson = R"({"accounts": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountsNotArray"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsItemNotString",
            .testJson = R"({"accounts": [123]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts'sItemNotString"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsItemInvalidString",
            .testJson = R"({"accounts": ["123"]})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts'sItemMalformed"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsEmptyArray",
            .testJson = R"({"accounts": []})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsProposedNotArray",
            .testJson = R"({"accounts_proposed": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts_proposedNotArray"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsProposedItemNotString",
            .testJson = R"({"accounts_proposed": [123]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts_proposed'sItemNotString"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsProposedItemInvalidString",
            .testJson = R"({"accounts_proposed": ["123"]})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts_proposed'sItemMalformed"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsProposedEmptyArray",
            .testJson = R"({"accounts_proposed": []})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts_proposed malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamsNotArray",
            .testJson = R"({"streams": 1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "streamsNotArray"
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamNotString",
            .testJson = R"({"streams": [1]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "streamNotString"
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamNotValid",
            .testJson = R"({"streams": ["1"]})",
            .expectedError = "malformedStream",
            .expectedErrorMessage = "Stream malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamPeerStatusNotSupport",
            .testJson = R"({"streams": ["peer_status"]})",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamConsensusNotSupport",
            .testJson = R"({"streams": ["consensus"]})",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamServerNotSupport",
            .testJson = R"({"streams": ["server"]})",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksNotArray",
            .testJson = R"({"books": "1"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "booksNotArray"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemNotObject",
            .testJson = R"({"books": ["1"]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "booksItemNotObject"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemMissingTakerPays",
            .testJson = R"({"books": [{"taker_gets": {"currency": "XRP"}}]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Missing field 'taker_pays'"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemMissingTakerGets",
            .testJson = R"({"books": [{"taker_pays": {"currency": "XRP"}}]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Missing field 'taker_gets'"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsNotObject",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": "USD"
                    }
                ]
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Field 'taker_gets' is not an object"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysNotObject",
            .testJson = R"({
                "books": [
                    {
                        "taker_gets": 
                        {
                            "currency": "XRP"
                        },
                        "taker_pays": "USD"
                    }
                ]
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Field 'taker_pays' is not an object"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysMissingCurrency",
            .testJson = R"({
                "books": [
                    {
                        "taker_gets": 
                        {
                            "currency": "XRP"
                        },
                        "taker_pays": {}
                    }
                ]
            })",
            .expectedError = "srcCurMalformed",
            .expectedErrorMessage = "Source currency is malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsMissingCurrency",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {}
                    }
                ]
            })",
            .expectedError = "dstAmtMalformed",
            .expectedErrorMessage = "Destination amount/currency/issuer is malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysCurrencyNotString",
            .testJson = R"({
                "books": [
                    {
                        "taker_gets": 
                        {
                            "currency": "XRP"
                        },
                        "taker_pays": {
                            "currency": 1,
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })",
            .expectedError = "srcCurMalformed",
            .expectedErrorMessage = "Source currency is malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsCurrencyNotString",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": 1,
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })",
            .expectedError = "dstAmtMalformed",
            .expectedErrorMessage = "Destination amount/currency/issuer is malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysInvalidCurrency",
            .testJson = R"({
                "books": [
                    {
                        "taker_gets": 
                        {
                            "currency": "XRP"
                        },
                        "taker_pays": {
                            "currency": "XXXXXX",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })",
            .expectedError = "srcCurMalformed",
            .expectedErrorMessage = "Source currency is malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsInvalidCurrency",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "xxxxxxx",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })",
            .expectedError = "dstAmtMalformed",
            .expectedErrorMessage = "Destination amount/currency/issuer is malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysMissingIssuer",
            .testJson = R"({
                "books": [
                    {
                        "taker_gets": 
                        {
                            "currency": "XRP"
                        },
                        "taker_pays": {
                            "currency": "USD"
                        }
                    }
                ]
            })",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Invalid field 'taker_pays.issuer', expected non-XRP issuer."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsMissingIssuer",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD"
                        }
                    }
                ]
            })",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Invalid field 'taker_gets.issuer', expected non-XRP issuer."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysIssuerNotString",
            .testJson = R"({
                "books": [
                    {
                        "taker_gets": 
                        {
                            "currency": "XRP"
                        },
                        "taker_pays": {
                            "currency": "USD",
                            "issuer": 1
                        }
                    }
                ]
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "takerPaysIssuerNotString"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsIssuerNotString",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": 1
                        }
                    }
                ]
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "taker_gets.issuer should be string"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysInvalidIssuer",
            .testJson = R"({
                "books": [
                    {
                        "taker_gets": 
                        {
                            "currency": "XRP"
                        },
                        "taker_pays": {
                            "currency": "USD",
                            "issuer": "123"
                        }
                    }
                ]
            })",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Source issuer is malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsInvalidIssuer",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": "123"
                        }
                    }
                ]
            })",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Invalid field 'taker_gets.issuer', bad issuer."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsXRPHasIssuer",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "USD",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        },
                        "taker_gets": {
                            "currency": "XRP",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Unneeded field 'taker_gets.issuer' for XRP currency specification."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysXRPHasIssuer",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Unneeded field 'taker_pays.issuer' for XRP currency specification."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemBadMartket",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "XRP"
                        }
                    }
                ]
            })",
            .expectedError = "badMarket",
            .expectedErrorMessage = "badMarket"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemInvalidSnapshot",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        },
                        "snapshot": 0
                    }
                ]
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "snapshotNotBool"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemInvalidBoth",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        },
                        "both": 0
                    }
                ]
            })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "bothNotBool"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemInvalidTakerNotString",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        },
                        "taker": 0
                    }
                ]
            })",
            .expectedError = "badIssuer",
            .expectedErrorMessage = "Issuer account malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemInvalidTaker",
            .testJson = R"({
                "books": [
                    {
                        "taker_pays": 
                        {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        },
                        "taker": "xxxxxxx"
                    }
                ]
            })",
            .expectedError = "badIssuer",
            .expectedErrorMessage = "Issuer account malformed."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCSubscribe,
    SubscribeParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(SubscribeParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCSubscribeHandlerTest, EmptyResponse)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(json::parse(R"({})"), Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, StreamsWithoutLedger)
{
    // these streams don't return response
    auto const input = json::parse(
        R"({
            "streams": ["transactions_proposed","transactions","validations","manifests","book_changes"]
        })"
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};
        EXPECT_CALL(*mockSubscriptionManagerPtr, subTransactions);
        EXPECT_CALL(*mockSubscriptionManagerPtr, subValidation);
        EXPECT_CALL(*mockSubscriptionManagerPtr, subManifest);
        EXPECT_CALL(*mockSubscriptionManagerPtr, subBookChanges);
        EXPECT_CALL(*mockSubscriptionManagerPtr, subProposedTransactions);

        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, StreamsLedger)
{
    static auto constexpr expectedOutput =
        R"({      
            "validated_ledgers":"10-30",
            "ledger_index":30,
            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time":0,
            "fee_base":1,
            "reserve_base":3,
            "reserve_inc":2
        })";

    auto const input = json::parse(
        R"({
            "streams": ["ledger"]
        })"
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};

        EXPECT_CALL(*mockSubscriptionManagerPtr, subLedger)
            .WillOnce(testing::Return(boost::json::parse(expectedOutput).as_object()));

        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object(), json::parse(expectedOutput));
    });
}

TEST_F(RPCSubscribeHandlerTest, Accounts)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "accounts": ["{}","{}","{}"]
        }})",
        ACCOUNT,
        ACCOUNT2,
        ACCOUNT2
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};

        EXPECT_CALL(*mockSubscriptionManagerPtr, subAccount(GetAccountIDWithString(ACCOUNT), session_));
        EXPECT_CALL(*mockSubscriptionManagerPtr, subAccount(GetAccountIDWithString(ACCOUNT2), session_)).Times(2);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, AccountsProposed)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "accounts_proposed": ["{}","{}","{}"]
        }})",
        ACCOUNT,
        ACCOUNT2,
        ACCOUNT2
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};

        EXPECT_CALL(*mockSubscriptionManagerPtr, subProposedAccount(GetAccountIDWithString(ACCOUNT), session_));
        EXPECT_CALL(*mockSubscriptionManagerPtr, subProposedAccount(GetAccountIDWithString(ACCOUNT2), session_))
            .Times(2);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, JustBooks)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "books": 
            [
                {{
                    "taker_pays": 
                    {{
                        "currency": "XRP"
                    }},
                    "taker_gets": 
                    {{
                        "currency": "USD",
                        "issuer": "{}"
                    }}
                }}
            ]
        }})",
        ACCOUNT
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};
        EXPECT_CALL(*mockSubscriptionManagerPtr, subBook);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, BooksBothSet)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "books": 
            [
                {{
                    "taker_pays": 
                    {{
                        "currency": "XRP"
                    }},
                    "taker_gets": 
                    {{
                        "currency": "USD",
                        "issuer": "{}"
                    }},
                    "both": true
                }}
            ]
        }})",
        ACCOUNT
    ));
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};
        EXPECT_CALL(*mockSubscriptionManagerPtr, subBook).Times(2);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, BooksBothSnapshotSet)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "books": 
            [
                {{
                    "taker_gets": 
                    {{
                        "currency": "XRP"
                    }},
                    "taker_pays": 
                    {{
                        "currency": "USD",
                        "issuer": "{}"
                    }},
                    "both": true,
                    "snapshot": true
                }}
            ]
        }})",
        ACCOUNT
    ));
    backend->setRange(MINSEQ, MAXSEQ);

    auto const issuer = GetAccountIDWithString(ACCOUNT);

    auto const getsXRPPaysUSDBook = getBookBase(std::get<ripple::Book>(
        rpc::parseBook(ripple::to_currency("USD"), issuer, ripple::xrpCurrency(), ripple::xrpAccount())
    ));

    auto const reversedBook = getBookBase(std::get<ripple::Book>(
        rpc::parseBook(ripple::xrpCurrency(), ripple::xrpAccount(), ripple::to_currency("USD"), issuer)
    ));

    ON_CALL(*backend, doFetchSuccessorKey(getsXRPPaysUSDBook, MAXSEQ, _))
        .WillByDefault(Return(ripple::uint256{PAYS20USDGETS10XRPBOOKDIR}));

    ON_CALL(*backend, doFetchSuccessorKey(ripple::uint256{PAYS20USDGETS10XRPBOOKDIR}, MAXSEQ, _))
        .WillByDefault(Return(std::nullopt));

    ON_CALL(*backend, doFetchSuccessorKey(reversedBook, MAXSEQ, _))
        .WillByDefault(Return(ripple::uint256{PAYS20XRPGETS10USDBOOKDIR}));

    EXPECT_CALL(*backend, doFetchSuccessorKey).Times(4);

    // 2 book dirs + 2 issuer global freeze + 2 transferRate + 1 owner root + 1 fee
    EXPECT_CALL(*backend, doFetchLedgerObject).Times(8);

    auto const indexes = std::vector<ripple::uint256>(10, ripple::uint256{INDEX2});
    ON_CALL(*backend, doFetchLedgerObject(ripple::uint256{PAYS20USDGETS10XRPBOOKDIR}, MAXSEQ, _))
        .WillByDefault(Return(CreateOwnerDirLedgerObject(indexes, INDEX1).getSerializer().peekData()));

    // for reverse
    auto const indexes2 = std::vector<ripple::uint256>(10, ripple::uint256{INDEX1});
    ON_CALL(*backend, doFetchLedgerObject(ripple::uint256{PAYS20XRPGETS10USDBOOKDIR}, MAXSEQ, _))
        .WillByDefault(Return(CreateOwnerDirLedgerObject(indexes2, INDEX2).getSerializer().peekData()));

    // offer owner account root
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT2)).key, MAXSEQ, _))
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT2, 0, 2, 200, 2, INDEX1, 2).getSerializer().peekData()));

    // issuer account root
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key, MAXSEQ, _))
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2).getSerializer().peekData()));

    // fee
    auto feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::fees().key, MAXSEQ, _)).WillByDefault(Return(feeBlob));

    auto const gets10XRPPays20USDOffer = CreateOfferLedgerObject(
        ACCOUNT2,
        10,
        20,
        ripple::to_string(ripple::xrpCurrency()),
        ripple::to_string(ripple::to_currency("USD")),
        toBase58(ripple::xrpAccount()),
        ACCOUNT,
        PAYS20USDGETS10XRPBOOKDIR
    );

    // for reverse
    // offer owner is USD issuer
    auto const gets10USDPays20XRPOffer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT,
        toBase58(ripple::xrpAccount()),
        PAYS20XRPGETS10USDBOOKDIR
    );

    std::vector<Blob> const bbs(10, gets10XRPPays20USDOffer.getSerializer().peekData());
    ON_CALL(*backend, doFetchLedgerObjects(indexes, MAXSEQ, _)).WillByDefault(Return(bbs));

    // for reverse
    std::vector<Blob> const bbs2(10, gets10USDPays20XRPOffer.getSerializer().peekData());
    ON_CALL(*backend, doFetchLedgerObjects(indexes2, MAXSEQ, _)).WillByDefault(Return(bbs2));

    EXPECT_CALL(*backend, doFetchLedgerObjects).Times(2);

    static auto const expectedOffer = fmt::format(
        R"({{
            "Account":"{}",
            "BookDirectory":"{}",
            "BookNode":"0",
            "Flags":0,
            "LedgerEntryType":"Offer",
            "OwnerNode":"0",
            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
            "PreviousTxnLgrSeq":0,
            "Sequence":0,
            "TakerGets":"10",
            "TakerPays":
            {{
                "currency":"USD",
                "issuer":"{}",
                "value":"20"
            }},
            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
            "owner_funds":"193",
            "quality":"2"
        }})",
        ACCOUNT2,
        PAYS20USDGETS10XRPBOOKDIR,
        ACCOUNT
    );
    static auto const expectedReversedOffer = fmt::format(
        R"({{
            "Account":"{}",
            "BookDirectory":"{}",
            "BookNode":"0",
            "Flags":0,
            "LedgerEntryType":"Offer",
            "OwnerNode":"0",
            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
            "PreviousTxnLgrSeq":0,
            "Sequence":0,
            "TakerGets":
            {{
                "currency":"USD",
                "issuer":"{}",
                "value":"10"
            }},
            "TakerPays":"20",
            "index":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
            "owner_funds":"10",
            "quality":"2"
        }})",
        ACCOUNT,
        PAYS20XRPGETS10USDBOOKDIR,
        ACCOUNT
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};
        EXPECT_CALL(*mockSubscriptionManagerPtr, subBook).Times(2);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("bids").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("asks").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("bids").as_array()[0].as_object(), json::parse(expectedOffer));
        EXPECT_EQ(output.result->as_object().at("asks").as_array()[0].as_object(), json::parse(expectedReversedOffer));
    });
}

TEST_F(RPCSubscribeHandlerTest, BooksBothUnsetSnapshotSet)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "books": 
            [
                {{
                    "taker_gets": 
                    {{
                        "currency": "XRP"
                    }},
                    "taker_pays": 
                    {{
                        "currency": "USD",
                        "issuer": "{}"
                    }},
                    "snapshot": true
                }}
            ]
        }})",
        ACCOUNT
    ));
    backend->setRange(MINSEQ, MAXSEQ);

    auto const issuer = GetAccountIDWithString(ACCOUNT);

    auto const getsXRPPaysUSDBook = getBookBase(std::get<ripple::Book>(
        rpc::parseBook(ripple::to_currency("USD"), issuer, ripple::xrpCurrency(), ripple::xrpAccount())
    ));

    auto const reversedBook = getBookBase(std::get<ripple::Book>(
        rpc::parseBook(ripple::xrpCurrency(), ripple::xrpAccount(), ripple::to_currency("USD"), issuer)
    ));

    ON_CALL(*backend, doFetchSuccessorKey(getsXRPPaysUSDBook, MAXSEQ, _))
        .WillByDefault(Return(ripple::uint256{PAYS20USDGETS10XRPBOOKDIR}));

    ON_CALL(*backend, doFetchSuccessorKey(ripple::uint256{PAYS20USDGETS10XRPBOOKDIR}, MAXSEQ, _))
        .WillByDefault(Return(std::nullopt));

    ON_CALL(*backend, doFetchSuccessorKey(reversedBook, MAXSEQ, _))
        .WillByDefault(Return(ripple::uint256{PAYS20XRPGETS10USDBOOKDIR}));

    EXPECT_CALL(*backend, doFetchSuccessorKey).Times(2);

    EXPECT_CALL(*backend, doFetchLedgerObject).Times(5);

    auto const indexes = std::vector<ripple::uint256>(10, ripple::uint256{INDEX2});
    ON_CALL(*backend, doFetchLedgerObject(ripple::uint256{PAYS20USDGETS10XRPBOOKDIR}, MAXSEQ, _))
        .WillByDefault(Return(CreateOwnerDirLedgerObject(indexes, INDEX1).getSerializer().peekData()));

    // for reverse
    auto const indexes2 = std::vector<ripple::uint256>(10, ripple::uint256{INDEX1});
    ON_CALL(*backend, doFetchLedgerObject(ripple::uint256{PAYS20XRPGETS10USDBOOKDIR}, MAXSEQ, _))
        .WillByDefault(Return(CreateOwnerDirLedgerObject(indexes2, INDEX2).getSerializer().peekData()));

    // offer owner account root
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT2)).key, MAXSEQ, _))
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT2, 0, 2, 200, 2, INDEX1, 2).getSerializer().peekData()));

    // issuer account root
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key, MAXSEQ, _))
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2).getSerializer().peekData()));

    // fee
    auto feeBlob = CreateLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*backend, doFetchLedgerObject(ripple::keylet::fees().key, MAXSEQ, _)).WillByDefault(Return(feeBlob));

    auto const gets10XRPPays20USDOffer = CreateOfferLedgerObject(
        ACCOUNT2,
        10,
        20,
        ripple::to_string(ripple::xrpCurrency()),
        ripple::to_string(ripple::to_currency("USD")),
        toBase58(ripple::xrpAccount()),
        ACCOUNT,
        PAYS20USDGETS10XRPBOOKDIR
    );

    // for reverse
    // offer owner is USD issuer
    auto const gets10USDPays20XRPOffer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT,
        toBase58(ripple::xrpAccount()),
        PAYS20XRPGETS10USDBOOKDIR
    );

    std::vector<Blob> const bbs(10, gets10XRPPays20USDOffer.getSerializer().peekData());
    ON_CALL(*backend, doFetchLedgerObjects(indexes, MAXSEQ, _)).WillByDefault(Return(bbs));

    // for reverse
    std::vector<Blob> const bbs2(10, gets10USDPays20XRPOffer.getSerializer().peekData());
    ON_CALL(*backend, doFetchLedgerObjects(indexes2, MAXSEQ, _)).WillByDefault(Return(bbs2));

    EXPECT_CALL(*backend, doFetchLedgerObjects);

    static auto const expectedOffer = fmt::format(
        R"({{
            "Account":"{}",
            "BookDirectory":"{}",
            "BookNode":"0",
            "Flags":0,
            "LedgerEntryType":"Offer",
            "OwnerNode":"0",
            "PreviousTxnID":"0000000000000000000000000000000000000000000000000000000000000000",
            "PreviousTxnLgrSeq":0,
            "Sequence":0,
            "TakerGets":"10",
            "TakerPays":
            {{
                "currency":"USD",
                "issuer":"{}",
                "value":"20"
            }},
            "index":"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
            "owner_funds":"193",
            "quality":"2"
        }})",
        ACCOUNT2,
        PAYS20USDGETS10XRPBOOKDIR,
        ACCOUNT
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};
        EXPECT_CALL(*mockSubscriptionManagerPtr, subBook);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("offers").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("offers").as_array()[0].as_object(), json::parse(expectedOffer));
    });
}

TEST_F(RPCSubscribeHandlerTest, APIVersion)
{
    auto const input = json::parse(
        R"({
            "streams": ["transactions_proposed"]
        })"
    );
    auto const apiVersion = 2;
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{SubscribeHandler{backend, mockSubscriptionManagerPtr}};
        EXPECT_CALL(*mockSubscriptionManagerPtr, subProposedTransactions);
        EXPECT_CALL(*mockSession_, setApiSubversion(apiVersion));
        auto const output =
            handler.process(input, Context{.yield = yield, .session = session_, .apiVersion = apiVersion});
        ASSERT_TRUE(output);
        // EXPECT_EQ(session_->apiSubVersion, apiVersion);
    });
}

TEST(RPCSubscribeHandlerSpecTest, DeprecatedFields)
{
    boost::json::value const json{
        {"streams", ACCOUNT},
        {"accounts", {123}},
        {"accounts_proposed", "abc"},
        {"books", "1"},
        {"user", "some"},
        {"password", "secret"},
        {"rt_accounts", true}
    };
    auto const spec = SubscribeHandler::spec(2);
    auto const warnings = spec.check(json);
    ASSERT_EQ(warnings.size(), 1);
    auto const& warning = warnings[0];
    ASSERT_TRUE(warning.is_object());
    auto const obj = warning.as_object();
    ASSERT_TRUE(obj.contains("id"));
    ASSERT_TRUE(obj.contains("message"));
    EXPECT_EQ(obj.at("id").as_int64(), static_cast<int64_t>(WarningCode::warnRPC_DEPRECATED));
    auto const& message = obj.at("message").as_string();
    for (auto const& field : {"user", "password", "rt_accounts"}) {
        EXPECT_NE(message.find(fmt::format("Field '{}' is deprecated", field)), std::string::npos) << message;
    }
}
