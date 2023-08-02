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
#include <rpc/handlers/Unsubscribe.h>
#include <util/Fixtures.h>
#include <util/MockSubscriptionManager.h>
#include <util/MockWsBase.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

using TestUnsubscribeHandler = BaseUnsubscribeHandler<MockSubscriptionManager>;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";

class RPCUnsubscribeTest : public HandlerBaseTest, public MockSubscriptionManagerTest
{
protected:
    void
    SetUp() override
    {
        HandlerBaseTest::SetUp();
        MockSubscriptionManagerTest::SetUp();
        clio::Config cfg;
        util::TagDecoratorFactory tagDecoratorFactory{cfg};
        session_ = std::make_shared<MockSession>(tagDecoratorFactory);
    }
    void
    TearDown() override
    {
        MockSubscriptionManagerTest::TearDown();
        HandlerBaseTest::TearDown();
    }

    std::shared_ptr<SubscriptionManager> subManager_;
    std::shared_ptr<Server::ConnectionBase> session_;
};

struct UnsubscribeParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct UnsubscribeParameterTest : public RPCUnsubscribeTest, public WithParamInterface<UnsubscribeParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<UnsubscribeParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<UnsubscribeParamTestCaseBundle>{
        UnsubscribeParamTestCaseBundle{
            "AccountsNotArray",
            R"({"accounts": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})",
            "invalidParams",
            "accountsNotArray"},
        UnsubscribeParamTestCaseBundle{
            "AccountsItemNotString", R"({"accounts": [123]})", "invalidParams", "accounts'sItemNotString"},
        UnsubscribeParamTestCaseBundle{
            "AccountsItemInvalidString", R"({"accounts": ["123"]})", "actMalformed", "accounts'sItemMalformed"},
        UnsubscribeParamTestCaseBundle{
            "AccountsEmptyArray", R"({"accounts": []})", "actMalformed", "accounts malformed."},
        UnsubscribeParamTestCaseBundle{
            "AccountsProposedNotArray",
            R"({"accounts_proposed": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})",
            "invalidParams",
            "accounts_proposedNotArray"},
        UnsubscribeParamTestCaseBundle{
            "AccountsProposedItemNotString",
            R"({"accounts_proposed": [123]})",
            "invalidParams",
            "accounts_proposed'sItemNotString"},
        UnsubscribeParamTestCaseBundle{
            "AccountsProposedItemInvalidString",
            R"({"accounts_proposed": ["123"]})",
            "actMalformed",
            "accounts_proposed'sItemMalformed"},
        UnsubscribeParamTestCaseBundle{
            "AccountsProposedEmptyArray",
            R"({"accounts_proposed": []})",
            "actMalformed",
            "accounts_proposed malformed."},
        UnsubscribeParamTestCaseBundle{"StreamsNotArray", R"({"streams": 1})", "invalidParams", "streamsNotArray"},
        UnsubscribeParamTestCaseBundle{"StreamNotString", R"({"streams": [1]})", "invalidParams", "streamNotString"},
        UnsubscribeParamTestCaseBundle{
            "StreamNotValid", R"({"streams": ["1"]})", "malformedStream", "Stream malformed."},
        UnsubscribeParamTestCaseBundle{"BooksNotArray", R"({"books": "1"})", "invalidParams", "booksNotArray"},
        UnsubscribeParamTestCaseBundle{
            "BooksItemNotObject", R"({"books": ["1"]})", "invalidParams", "booksItemNotObject"},
        UnsubscribeParamTestCaseBundle{
            "BooksItemMissingTakerPays",
            R"({"books": [{"taker_gets": {"currency": "XRP"}}]})",
            "invalidParams",
            "Missing field 'taker_pays'"},
        UnsubscribeParamTestCaseBundle{
            "BooksItemMissingTakerGets",
            R"({"books": [{"taker_pays": {"currency": "XRP"}}]})",
            "invalidParams",
            "Missing field 'taker_gets'"},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerGetsNotObject",
            R"({
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
            "invalidParams",
            "Field 'taker_gets' is not an object"},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerPaysNotObject",
            R"({
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
            "invalidParams",
            "Field 'taker_pays' is not an object"},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerPaysMissingCurrency",
            R"({
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
            "srcCurMalformed",
            "Source currency is malformed."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerGetsMissingCurrency",
            R"({
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
            "dstAmtMalformed",
            "Destination amount/currency/issuer is malformed."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerPaysCurrencyNotString",
            R"({
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
            "srcCurMalformed",
            "Source currency is malformed."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerGetsCurrencyNotString",
            R"({
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
            "dstAmtMalformed",
            "Destination amount/currency/issuer is malformed."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerPaysInvalidCurrency",
            R"({
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
            "srcCurMalformed",
            "Source currency is malformed."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerGetsInvalidCurrency",
            R"({
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
            "dstAmtMalformed",
            "Destination amount/currency/issuer is malformed."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerPaysMissingIssuer",
            R"({
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
            "srcIsrMalformed",
            "Invalid field 'taker_pays.issuer', expected non-XRP issuer."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerGetsMissingIssuer",
            R"({
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
            "dstIsrMalformed",
            "Invalid field 'taker_gets.issuer', expected non-XRP issuer."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerPaysIssuerNotString",
            R"({
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
            "invalidParams",
            "takerPaysIssuerNotString"},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerGetsIssuerNotString",
            R"({
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
            "invalidParams",
            "taker_gets.issuer should be string"},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerPaysInvalidIssuer",
            R"({
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
            "srcIsrMalformed",
            "Source issuer is malformed."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerGetsInvalidIssuer",
            R"({
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
            "dstIsrMalformed",
            "Invalid field 'taker_gets.issuer', bad issuer."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerGetsXRPHasIssuer",
            R"({
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
            "dstIsrMalformed",
            "Unneeded field 'taker_gets.issuer' for XRP currency "
            "specification."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemTakerPaysXRPHasIssuer",
            R"({
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
            "srcIsrMalformed",
            "Unneeded field 'taker_pays.issuer' for XRP currency "
            "specification."},
        UnsubscribeParamTestCaseBundle{
            "BooksItemBadMartket",
            R"({
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
            "badMarket",
            "badMarket"},
        UnsubscribeParamTestCaseBundle{
            "BooksItemInvalidBoth",
            R"({
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
            "invalidParams",
            "bothNotBool"},
        UnsubscribeParamTestCaseBundle{
            "StreamPeerStatusNotSupport",
            R"({"streams": ["peer_status"]})",
            "reportingUnsupported",
            "Requested operation not supported by reporting mode server"},
        UnsubscribeParamTestCaseBundle{
            "StreamConsensusNotSupport",
            R"({"streams": ["consensus"]})",
            "reportingUnsupported",
            "Requested operation not supported by reporting mode server"},
        UnsubscribeParamTestCaseBundle{
            "StreamServerNotSupport",
            R"({"streams": ["server"]})",
            "reportingUnsupported",
            "Requested operation not supported by reporting mode server"},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCUnsubscribe,
    UnsubscribeParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    UnsubscribeParameterTest::NameGenerator{});

TEST_P(UnsubscribeParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TestUnsubscribeHandler{mockBackendPtr, mockSubscriptionManagerPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCUnsubscribeTest, EmptyResponse)
{
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TestUnsubscribeHandler{mockBackendPtr, mockSubscriptionManagerPtr}};
        auto const output = handler.process(json::parse(R"({})"), Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, Streams)
{
    auto const input = json::parse(
        R"({
            "streams": ["transactions_proposed","transactions","validations","manifests","book_changes","ledger"]
        })");

    MockSubscriptionManager* rawSubscriptionManagerPtr =
        static_cast<MockSubscriptionManager*>(mockSubscriptionManagerPtr.get());
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubLedger).Times(1);
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubTransactions).Times(1);
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubValidation).Times(1);
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubManifest).Times(1);
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubBookChanges).Times(1);
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubProposedTransactions).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TestUnsubscribeHandler{mockBackendPtr, mockSubscriptionManagerPtr}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, Accounts)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "accounts": ["{}","{}"]
        }})",
        ACCOUNT,
        ACCOUNT2));

    MockSubscriptionManager* rawSubscriptionManagerPtr =
        static_cast<MockSubscriptionManager*>(mockSubscriptionManagerPtr.get());
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubAccount(RPC::accountFromStringStrict(ACCOUNT).value(), _)).Times(1);
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubAccount(RPC::accountFromStringStrict(ACCOUNT2).value(), _)).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TestUnsubscribeHandler{mockBackendPtr, mockSubscriptionManagerPtr}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
    });
}

TEST_F(RPCUnsubscribeTest, AccountsProposed)
{
    auto const input = json::parse(fmt::format(
        R"({{
            "accounts_proposed": ["{}","{}"]
        }})",
        ACCOUNT,
        ACCOUNT2));

    MockSubscriptionManager* rawSubscriptionManagerPtr =
        static_cast<MockSubscriptionManager*>(mockSubscriptionManagerPtr.get());
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubProposedAccount(RPC::accountFromStringStrict(ACCOUNT).value(), _))
        .Times(1);
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubProposedAccount(RPC::accountFromStringStrict(ACCOUNT2).value(), _))
        .Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TestUnsubscribeHandler{mockBackendPtr, mockSubscriptionManagerPtr}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
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
        ACCOUNT));

    auto const parsedBookMaybe = RPC::parseBook(input.as_object().at("books").as_array()[0].as_object());
    auto const book = std::get<ripple::Book>(parsedBookMaybe);

    MockSubscriptionManager* rawSubscriptionManagerPtr =
        static_cast<MockSubscriptionManager*>(mockSubscriptionManagerPtr.get());
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubBook(book, _)).Times(1);
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubBook(ripple::reversed(book), _)).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TestUnsubscribeHandler{mockBackendPtr, mockSubscriptionManagerPtr}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
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
        ACCOUNT));

    auto const parsedBookMaybe = RPC::parseBook(input.as_object().at("books").as_array()[0].as_object());
    auto const book = std::get<ripple::Book>(parsedBookMaybe);

    MockSubscriptionManager* rawSubscriptionManagerPtr =
        static_cast<MockSubscriptionManager*>(mockSubscriptionManagerPtr.get());
    EXPECT_CALL(*rawSubscriptionManagerPtr, unsubBook(book, _)).Times(1);

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{TestUnsubscribeHandler{mockBackendPtr, mockSubscriptionManagerPtr}};
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
    });
}
