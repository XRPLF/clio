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

#include "rpc/Errors.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/Unsubscribe.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/MockWsBase.hpp"
#include "util/NameGenerator.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/Book.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;
using namespace feed;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";

struct RPCUnsubscribeTest : HandlerBaseTest {
    web::SubscriptionContextPtr session_ = std::make_shared<MockSession>();
    StrictMockSubscriptionManagerSharedPtr mockSubscriptionManagerPtr;
};

struct UnsubscribeParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct UnsubscribeParameterTest : public RPCUnsubscribeTest,
                                  public WithParamInterface<UnsubscribeParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<UnsubscribeParamTestCaseBundle>{
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsNotArray",
            .testJson = R"({"accounts": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountsNotArray"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsItemNotString",
            .testJson = R"({"accounts": [123]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts'sItemNotString"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsItemInvalidString",
            .testJson = R"({"accounts": ["123"]})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts'sItemMalformed"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsEmptyArray",
            .testJson = R"({"accounts": []})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsProposedNotArray",
            .testJson = R"({"accounts_proposed": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts_proposedNotArray"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsProposedItemNotString",
            .testJson = R"({"accounts_proposed": [123]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts_proposed'sItemNotString"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsProposedItemInvalidString",
            .testJson = R"({"accounts_proposed": ["123"]})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts_proposed'sItemMalformed"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsProposedEmptyArray",
            .testJson = R"({"accounts_proposed": []})",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts_proposed malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamsNotArray",
            .testJson = R"({"streams": 1})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "streamsNotArray"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamNotString",
            .testJson = R"({"streams": [1]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "streamNotString"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamNotValid",
            .testJson = R"({"streams": ["1"]})",
            .expectedError = "malformedStream",
            .expectedErrorMessage = "Stream malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksNotArray",
            .testJson = R"({"books": "1"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "booksNotArray"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemNotObject",
            .testJson = R"({"books": ["1"]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "booksItemNotObject"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemMissingTakerPays",
            .testJson = R"({"books": [{"taker_gets": {"currency": "XRP"}}]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Missing field 'taker_pays'"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemMissingTakerGets",
            .testJson = R"({"books": [{"taker_pays": {"currency": "XRP"}}]})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Missing field 'taker_gets'"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsNotObject",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysNotObject",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysMissingCurrency",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsMissingCurrency",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysCurrencyNotString",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsCurrencyNotString",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysInvalidCurrency",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsInvalidCurrency",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysMissingIssuer",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsMissingIssuer",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysIssuerNotString",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsIssuerNotString",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysInvalidIssuer",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsInvalidIssuer",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsXRPHasIssuer",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysXRPHasIssuer",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemBadMartket",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemInvalidBoth",
            .testJson = R"({
                "books": 
                [
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
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamPeerStatusNotSupport",
            .testJson = R"({"streams": ["peer_status"]})",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamConsensusNotSupport",
            .testJson = R"({"streams": ["consensus"]})",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamServerNotSupport",
            .testJson = R"({"streams": ["server"]})",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCUnsubscribe,
    UnsubscribeParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(UnsubscribeParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCUnsubscribeTest, EmptyResponse)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr}};
        auto const output = handler.process(json::parse(R"({})"), Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, Streams)
{
    auto const input = json::parse(
        R"({
            "streams": ["transactions_proposed","transactions","validations","manifests","book_changes","ledger"]
        })"
    );

    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubLedger).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubTransactions).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubValidation).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubManifest).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubBookChanges).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubProposedTransactions).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, Accounts)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "accounts": ["{}","{}"]
        }})",
        ACCOUNT,
        ACCOUNT2
    ));

    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubAccount(rpc::accountFromStringStrict(ACCOUNT).value(), _)).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubAccount(rpc::accountFromStringStrict(ACCOUNT2).value(), _)).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, AccountsProposed)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "accounts_proposed": ["{}","{}"]
        }})",
        ACCOUNT,
        ACCOUNT2
    ));

    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubProposedAccount(rpc::accountFromStringStrict(ACCOUNT).value(), _))
        .Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubProposedAccount(rpc::accountFromStringStrict(ACCOUNT2).value(), _))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, Books)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "books": [
                {{
                    "taker_pays": {{
                        "currency": "XRP"
                    }},
                    "taker_gets": {{
                        "currency": "USD",
                        "issuer": "{}"
                    }},
                    "both": true
                }}
            ]
        }})",
        ACCOUNT
    ));

    auto const parsedBookMaybe = rpc::parseBook(input.as_object().at("books").as_array()[0].as_object());
    auto const book = std::get<ripple::Book>(parsedBookMaybe);

    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubBook(book, _)).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubBook(ripple::reversed(book), _)).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, SingleBooks)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "books": [
                {{
                    "taker_pays": {{
                        "currency": "XRP"
                    }},
                    "taker_gets": {{
                        "currency": "USD",
                        "issuer": "{}"
                    }}
                }}
            ]
        }})",
        ACCOUNT
    ));

    auto const parsedBookMaybe = rpc::parseBook(input.as_object().at("books").as_array()[0].as_object());
    auto const book = std::get<ripple::Book>(parsedBookMaybe);

    EXPECT_CALL(*mockSubscriptionManagerPtr, unsubBook(book, _)).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST(RPCUnsubscribeSpecTest, DeprecatedFields)
{
    boost::json::value const json{
        {"streams", 1},
        {"accounts", {"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"}},
        {"accounts_proposed", {"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"}},
        {"books", {}},
        {"url", "some_url"},
        {"rt_accounts", {"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"}},
        {"rt_transactions", {"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"}},
    };
    auto const spec = UnsubscribeHandler::spec(2);
    auto const warnings = spec.check(json);
    ASSERT_EQ(warnings.size(), 1);
    ASSERT_TRUE(warnings[0].is_object());
    auto const& warning = warnings[0].as_object();
    ASSERT_TRUE(warning.contains("id"));
    ASSERT_TRUE(warning.contains("message"));
    EXPECT_EQ(warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::warnRPC_DEPRECATED));
    for (auto const& field : {"url", "rt_accounts", "rt_accounts"}) {
        EXPECT_NE(
            warning.at("message").as_string().find(fmt::format("Field '{}' is deprecated.", field)), std::string::npos
        ) << warning;
    }
}
