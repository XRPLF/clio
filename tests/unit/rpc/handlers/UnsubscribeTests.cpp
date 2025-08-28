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
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/Book.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace rpc;
using namespace data;
namespace json = boost::json;
using namespace testing;
using namespace feed;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";

}  // namespace

struct RPCUnsubscribeTest : HandlerBaseTest {
protected:
    web::SubscriptionContextPtr session_ = std::make_shared<MockSession>();
    StrictMockSubscriptionManagerSharedPtr mockSubscriptionManagerPtr_;
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
            .testJson = R"JSON({"accounts": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountsNotArray"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsItemNotString",
            .testJson = R"JSON({"accounts": [123]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts'sItemNotString"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsItemInvalidString",
            .testJson = R"JSON({"accounts": ["123"]})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts'sItemMalformed"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsEmptyArray",
            .testJson = R"JSON({"accounts": []})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsProposedNotArray",
            .testJson = R"JSON({"accounts_proposed": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts_proposedNotArray"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsProposedItemNotString",
            .testJson = R"JSON({"accounts_proposed": [123]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts_proposed'sItemNotString"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsProposedItemInvalidString",
            .testJson = R"JSON({"accounts_proposed": ["123"]})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts_proposed'sItemMalformed"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "AccountsProposedEmptyArray",
            .testJson = R"JSON({"accounts_proposed": []})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts_proposed malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamsNotArray",
            .testJson = R"JSON({"streams": 1})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "streamsNotArray"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamNotString",
            .testJson = R"JSON({"streams": [1]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "streamNotString"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamNotValid",
            .testJson = R"JSON({"streams": ["1"]})JSON",
            .expectedError = "malformedStream",
            .expectedErrorMessage = "Stream malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksNotArray",
            .testJson = R"JSON({"books": "1"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "booksNotArray"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemNotObject",
            .testJson = R"JSON({"books": ["1"]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "booksItemNotObject"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemMissingTakerPays",
            .testJson = R"JSON({"books": [{"taker_gets": {"currency": "XRP"}}]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Missing field 'taker_pays'"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemMissingTakerGets",
            .testJson = R"JSON({"books": [{"taker_pays": {"currency": "XRP"}}]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Missing field 'taker_gets'"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsNotObject",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "XRP"
                        },
                        "taker_gets": "USD"
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Field 'taker_gets' is not an object"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysNotObject",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_gets": {
                            "currency": "XRP"
                        },
                        "taker_pays": "USD"
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Field 'taker_pays' is not an object"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysMissingCurrency",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_gets": {
                            "currency": "XRP"
                        },
                        "taker_pays": {}
                    }
                ]
            })JSON",
            .expectedError = "srcCurMalformed",
            .expectedErrorMessage = "Source currency is malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsMissingCurrency",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "XRP"
                        },
                        "taker_gets": {}
                    }
                ]
            })JSON",
            .expectedError = "dstAmtMalformed",
            .expectedErrorMessage = "Destination amount/currency/issuer is malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysCurrencyNotString",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_gets": {
                            "currency": "XRP"
                        },
                        "taker_pays": {
                            "currency": 1,
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })JSON",
            .expectedError = "srcCurMalformed",
            .expectedErrorMessage = "Source currency is malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsCurrencyNotString",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": 1,
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })JSON",
            .expectedError = "dstAmtMalformed",
            .expectedErrorMessage = "Destination amount/currency/issuer is malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysInvalidCurrency",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_gets": {
                            "currency": "XRP"
                        },
                        "taker_pays": {
                            "currency": "XXXXXX",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })JSON",
            .expectedError = "srcCurMalformed",
            .expectedErrorMessage = "Source currency is malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsInvalidCurrency",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "xxxxxxx",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })JSON",
            .expectedError = "dstAmtMalformed",
            .expectedErrorMessage = "Destination amount/currency/issuer is malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysMissingIssuer",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_gets": {
                            "currency": "XRP"
                        },
                        "taker_pays": {
                            "currency": "USD"
                        }
                    }
                ]
            })JSON",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Invalid field 'taker_pays.issuer', expected non-XRP issuer."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsMissingIssuer",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD"
                        }
                    }
                ]
            })JSON",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Invalid field 'taker_gets.issuer', expected non-XRP issuer."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysIssuerNotString",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_gets": {
                            "currency": "XRP"
                        },
                        "taker_pays": {
                            "currency": "USD",
                            "issuer": 1
                        }
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "takerPaysIssuerNotString"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsIssuerNotString",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": 1
                        }
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "taker_gets.issuer should be string"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysInvalidIssuer",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_gets": {
                            "currency": "XRP"
                        },
                        "taker_pays": {
                            "currency": "USD",
                            "issuer": "123"
                        }
                    }
                ]
            })JSON",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Source issuer is malformed."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsInvalidIssuer",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": "123"
                        }
                    }
                ]
            })JSON",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Invalid field 'taker_gets.issuer', bad issuer."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerGetsXRPHasIssuer",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "USD",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        },
                        "taker_gets": {
                            "currency": "XRP",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })JSON",
            .expectedError = "dstIsrMalformed",
            .expectedErrorMessage = "Unneeded field 'taker_gets.issuer' for XRP currency specification."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemTakerPaysXRPHasIssuer",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "XRP",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        }
                    }
                ]
            })JSON",
            .expectedError = "srcIsrMalformed",
            .expectedErrorMessage = "Unneeded field 'taker_pays.issuer' for XRP currency specification."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemBadMartket",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "XRP"
                        }
                    }
                ]
            })JSON",
            .expectedError = "badMarket",
            .expectedErrorMessage = "badMarket"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "BooksItemInvalidBoth",
            .testJson = R"JSON({
                "books": [
                    {
                        "taker_pays": {
                            "currency": "XRP"
                        },
                        "taker_gets": {
                            "currency": "USD",
                            "issuer": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"
                        },
                        "both": 0
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "bothNotBool"
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamPeerStatusNotSupport",
            .testJson = R"JSON({"streams": ["peer_status"]})JSON",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamConsensusNotSupport",
            .testJson = R"JSON({"streams": ["consensus"]})JSON",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
        UnsubscribeParamTestCaseBundle{
            .testName = "StreamServerNotSupport",
            .testJson = R"JSON({"streams": ["server"]})JSON",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCUnsubscribe,
    UnsubscribeParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(UnsubscribeParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr_}};
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
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr_}};
        auto const output = handler.process(json::parse(R"JSON({})JSON"), Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, Streams)
{
    auto const input = json::parse(
        R"JSON({
            "streams": ["transactions_proposed", "transactions", "validations", "manifests", "book_changes", "ledger"]
        })JSON"
    );

    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubLedger).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubTransactions).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubValidation).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubManifest).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubBookChanges).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubProposedTransactions).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr_}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, Accounts)
{
    auto const input = json::parse(
        fmt::format(
            R"JSON({{
            "accounts": ["{}", "{}"]
        }})JSON",
            kACCOUNT,
            kACCOUNT2
        )
    );

    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubAccount(rpc::accountFromStringStrict(kACCOUNT).value(), _)).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubAccount(rpc::accountFromStringStrict(kACCOUNT2).value(), _))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr_}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, AccountsProposed)
{
    auto const input = json::parse(
        fmt::format(
            R"JSON({{
            "accounts_proposed": ["{}", "{}"]
        }})JSON",
            kACCOUNT,
            kACCOUNT2
        )
    );

    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubProposedAccount(rpc::accountFromStringStrict(kACCOUNT).value(), _))
        .Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubProposedAccount(rpc::accountFromStringStrict(kACCOUNT2).value(), _))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr_}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, Books)
{
    auto const input = json::parse(
        fmt::format(
            R"JSON({{
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
        }})JSON",
            kACCOUNT
        )
    );

    auto const parsedBookMaybe = rpc::parseBook(input.as_object().at("books").as_array()[0].as_object());
    auto const book = parsedBookMaybe.value();

    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubBook(book, _)).Times(1);
    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubBook(ripple::reversed(book), _)).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr_}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, SingleBooks)
{
    auto const input = json::parse(
        fmt::format(
            R"JSON({{
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
        }})JSON",
            kACCOUNT
        )
    );

    auto const parsedBookMaybe = rpc::parseBook(input.as_object().at("books").as_array()[0].as_object());
    auto const book = parsedBookMaybe.value();

    EXPECT_CALL(*mockSubscriptionManagerPtr_, unsubBook(book, _)).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{UnsubscribeHandler{mockSubscriptionManagerPtr_}};
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
    EXPECT_EQ(warning.at("id").as_int64(), static_cast<int64_t>(rpc::WarningCode::WarnRpcDeprecated));
    for (auto const& field : {"url", "rt_accounts", "rt_accounts"}) {
        EXPECT_NE(
            warning.at("message").as_string().find(fmt::format("Field '{}' is deprecated.", field)), std::string::npos
        ) << warning;
    }
}
