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
#include <rpc/handlers/Subscribe.h>
#include <subscriptions/SubscriptionManager.h>
#include <util/Fixtures.h>
#include <util/MockWsBase.h>
#include <util/TestObject.h>

#include <fmt/core.h>

#include <chrono>

using namespace std::chrono_literals;
using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto MINSEQ = 10;
constexpr static auto MAXSEQ = 30;
constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto PAYS20USDGETS10XRPBOOKDIR = "43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000";
constexpr static auto PAYS20XRPGETS10USDBOOKDIR = "7B1767D41DBCE79D9585CF9D0262A5FEC45E5206FF524F8B55071AFD498D0000";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto INDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

class RPCSubscribeHandlerTest : public HandlerBaseTest
{
protected:
    void
    SetUp() override
    {
        HandlerBaseTest::SetUp();
        clio::Config cfg;
        subManager_ = SubscriptionManager::make_SubscriptionManager(cfg, mockBackendPtr);
        util::TagDecoratorFactory tagDecoratorFactory{cfg};
        session_ = std::make_shared<MockSession>(tagDecoratorFactory);
    }
    void
    TearDown() override
    {
        HandlerBaseTest::TearDown();
    }

    std::shared_ptr<SubscriptionManager> subManager_;
    std::shared_ptr<Server::ConnectionBase> session_;
};

struct SubscribeParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct SubscribeParameterTest : public RPCSubscribeHandlerTest, public WithParamInterface<SubscribeParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<SubscribeParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<SubscribeParamTestCaseBundle>{
        SubscribeParamTestCaseBundle{
            "AccountsNotArray",
            R"({"accounts": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})",
            "invalidParams",
            "accountsNotArray"},
        SubscribeParamTestCaseBundle{
            "AccountsItemNotString", R"({"accounts": [123]})", "invalidParams", "accounts'sItemNotString"},
        SubscribeParamTestCaseBundle{
            "AccountsItemInvalidString", R"({"accounts": ["123"]})", "actMalformed", "accounts'sItemMalformed"},
        SubscribeParamTestCaseBundle{
            "AccountsEmptyArray", R"({"accounts": []})", "actMalformed", "accounts malformed."},
        SubscribeParamTestCaseBundle{
            "AccountsProposedNotArray",
            R"({"accounts_proposed": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})",
            "invalidParams",
            "accounts_proposedNotArray"},
        SubscribeParamTestCaseBundle{
            "AccountsProposedItemNotString",
            R"({"accounts_proposed": [123]})",
            "invalidParams",
            "accounts_proposed'sItemNotString"},
        SubscribeParamTestCaseBundle{
            "AccountsProposedItemInvalidString",
            R"({"accounts_proposed": ["123"]})",
            "actMalformed",
            "accounts_proposed'sItemMalformed"},
        SubscribeParamTestCaseBundle{
            "AccountsProposedEmptyArray",
            R"({"accounts_proposed": []})",
            "actMalformed",
            "accounts_proposed malformed."},
        SubscribeParamTestCaseBundle{"StreamsNotArray", R"({"streams": 1})", "invalidParams", "streamsNotArray"},
        SubscribeParamTestCaseBundle{"StreamNotString", R"({"streams": [1]})", "invalidParams", "streamNotString"},
        SubscribeParamTestCaseBundle{"StreamNotValid", R"({"streams": ["1"]})", "malformedStream", "Stream malformed."},
        SubscribeParamTestCaseBundle{"BooksNotArray", R"({"books": "1"})", "invalidParams", "booksNotArray"},
        SubscribeParamTestCaseBundle{
            "BooksItemNotObject", R"({"books": ["1"]})", "invalidParams", "booksItemNotObject"},
        SubscribeParamTestCaseBundle{
            "BooksItemMissingTakerPays",
            R"({"books": [{"taker_gets": {"currency": "XRP"}}]})",
            "invalidParams",
            "Missing field 'taker_pays'"},
        SubscribeParamTestCaseBundle{
            "BooksItemMissingTakerGets",
            R"({"books": [{"taker_pays": {"currency": "XRP"}}]})",
            "invalidParams",
            "Missing field 'taker_gets'"},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerGetsNotObject",
            R"({
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
            "invalidParams",
            "Field 'taker_gets' is not an object"},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerPaysNotObject",
            R"({
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
            "invalidParams",
            "Field 'taker_pays' is not an object"},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerPaysMissingCurrency",
            R"({
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
            "srcCurMalformed",
            "Source currency is malformed."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerGetsMissingCurrency",
            R"({
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
            "dstAmtMalformed",
            "Destination amount/currency/issuer is malformed."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerPaysCurrencyNotString",
            R"({
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
            "srcCurMalformed",
            "Source currency is malformed."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerGetsCurrencyNotString",
            R"({
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
            "dstAmtMalformed",
            "Destination amount/currency/issuer is malformed."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerPaysInvalidCurrency",
            R"({
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
            "srcCurMalformed",
            "Source currency is malformed."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerGetsInvalidCurrency",
            R"({
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
            "dstAmtMalformed",
            "Destination amount/currency/issuer is malformed."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerPaysMissingIssuer",
            R"({
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
            "srcIsrMalformed",
            "Invalid field 'taker_pays.issuer', expected non-XRP issuer."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerGetsMissingIssuer",
            R"({
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
            "dstIsrMalformed",
            "Invalid field 'taker_gets.issuer', expected non-XRP issuer."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerPaysIssuerNotString",
            R"({
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
            "invalidParams",
            "takerPaysIssuerNotString"},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerGetsIssuerNotString",
            R"({
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
            "invalidParams",
            "taker_gets.issuer should be string"},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerPaysInvalidIssuer",
            R"({
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
            "srcIsrMalformed",
            "Source issuer is malformed."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerGetsInvalidIssuer",
            R"({
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
            "dstIsrMalformed",
            "Invalid field 'taker_gets.issuer', bad issuer."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerGetsXRPHasIssuer",
            R"({
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
            "dstIsrMalformed",
            "Unneeded field 'taker_gets.issuer' for XRP currency "
            "specification."},
        SubscribeParamTestCaseBundle{
            "BooksItemTakerPaysXRPHasIssuer",
            R"({
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
            "srcIsrMalformed",
            "Unneeded field 'taker_pays.issuer' for XRP currency "
            "specification."},
        SubscribeParamTestCaseBundle{
            "BooksItemBadMartket",
            R"({
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
            "badMarket",
            "badMarket"},
        SubscribeParamTestCaseBundle{
            "BooksItemInvalidSnapshot",
            R"({
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
            "invalidParams",
            "snapshotNotBool"},
        SubscribeParamTestCaseBundle{
            "BooksItemInvalidBoth",
            R"({
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
            "invalidParams",
            "bothNotBool"},
        SubscribeParamTestCaseBundle{
            "BooksItemInvalidTakerNotString",
            R"({
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
            "invalidParams",
            "takerNotString"},
        SubscribeParamTestCaseBundle{
            "BooksItemInvalidTaker",
            R"({
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
            "actMalformed",
            "takerMalformed"},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCSubscribe,
    SubscribeParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    SubscribeParameterTest::NameGenerator{});

TEST_P(SubscribeParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{SubscribeHandler{mockBackendPtr, subManager_}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCSubscribeHandlerTest, EmptyResponse)
{
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{SubscribeHandler{mockBackendPtr, subManager_}};
        auto const output = handler.process(json::parse(R"({})"), Context{std::ref(yield), session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, StreamsWithoutLedger)
{
    // these streams don't return response
    auto const input = json::parse(
        R"({
            "streams": ["transactions_proposed","transactions","validations","manifests","book_changes"]
        })");
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{SubscribeHandler{mockBackendPtr, subManager_}};
        auto const output = handler.process(input, Context{std::ref(yield), session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
        std::this_thread::sleep_for(20ms);
        auto const report = subManager_->report();
        EXPECT_EQ(report.at("transactions_proposed").as_uint64(), 1);
        EXPECT_EQ(report.at("transactions").as_uint64(), 1);
        EXPECT_EQ(report.at("validations").as_uint64(), 1);
        EXPECT_EQ(report.at("manifests").as_uint64(), 1);
        EXPECT_EQ(report.at("book_changes").as_uint64(), 1);
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
            "fee_ref":4,
            "fee_base":1,
            "reserve_base":3,
            "reserve_inc":2
        })";
    mockBackendPtr->updateRange(MINSEQ);
    mockBackendPtr->updateRange(MAXSEQ);
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return valid ledgerinfo
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(MAXSEQ, _)).WillByDefault(Return(ledgerinfo));
    // fee
    auto feeBlob = CreateFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(feeBlob));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    // ledger stream returns information about the ledgers on hand and current
    // fee schedule.
    auto const input = json::parse(
        R"({
            "streams": ["ledger"]
        })");
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{SubscribeHandler{mockBackendPtr, subManager_}};
        auto const output = handler.process(input, Context{std::ref(yield), session_});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object(), json::parse(expectedOutput));
        std::this_thread::sleep_for(20ms);
        auto const report = subManager_->report();
        EXPECT_EQ(report.at("ledger").as_uint64(), 1);
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
        ACCOUNT2));
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{SubscribeHandler{mockBackendPtr, subManager_}};
        auto const output = handler.process(input, Context{std::ref(yield), session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
        std::this_thread::sleep_for(20ms);
        auto const report = subManager_->report();
        // filter the duplicates
        EXPECT_EQ(report.at("account").as_uint64(), 2);
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
        ACCOUNT2));
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{SubscribeHandler{mockBackendPtr, subManager_}};
        auto const output = handler.process(input, Context{std::ref(yield), session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
        std::this_thread::sleep_for(20ms);
        auto const report = subManager_->report();
        // filter the duplicates
        EXPECT_EQ(report.at("accounts_proposed").as_uint64(), 2);
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
        ACCOUNT));
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{SubscribeHandler{mockBackendPtr, subManager_}};
        auto const output = handler.process(input, Context{std::ref(yield), session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
        std::this_thread::sleep_for(20ms);
        auto const report = subManager_->report();
        EXPECT_EQ(report.at("books").as_uint64(), 1);
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
        ACCOUNT));
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{SubscribeHandler{mockBackendPtr, subManager_}};
        auto const output = handler.process(input, Context{std::ref(yield), session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().empty());
        std::this_thread::sleep_for(20ms);
        auto const report = subManager_->report();
        // original book + reverse book
        EXPECT_EQ(report.at("books").as_uint64(), 2);
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
        ACCOUNT));
    mockBackendPtr->updateRange(MINSEQ);
    mockBackendPtr->updateRange(MAXSEQ);
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    auto const issuer = GetAccountIDWithString(ACCOUNT);

    auto const getsXRPPaysUSDBook = getBookBase(std::get<ripple::Book>(RPC::parseBook(
        ripple::to_currency("USD"),  // pays
        issuer,
        ripple::xrpCurrency(),       // gets
        ripple::xrpAccount())));

    auto const reversedBook = getBookBase(std::get<ripple::Book>(RPC::parseBook(
        ripple::xrpCurrency(),       // pays
        ripple::xrpAccount(),
        ripple::to_currency("USD"),  // gets
        issuer)));

    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(getsXRPPaysUSDBook, MAXSEQ, _))
        .WillByDefault(Return(ripple::uint256{PAYS20USDGETS10XRPBOOKDIR}));

    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(ripple::uint256{PAYS20USDGETS10XRPBOOKDIR}, MAXSEQ, _))
        .WillByDefault(Return(std::nullopt));

    ON_CALL(*rawBackendPtr, doFetchSuccessorKey(reversedBook, MAXSEQ, _))
        .WillByDefault(Return(ripple::uint256{PAYS20XRPGETS10USDBOOKDIR}));

    EXPECT_CALL(*rawBackendPtr, doFetchSuccessorKey).Times(4);

    // 2 book dirs + 2 issuer global freeze + 2 transferRate + 1 owner root + 1 fee
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(8);

    auto const indexes = std::vector<ripple::uint256>(10, ripple::uint256{INDEX2});
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::uint256{PAYS20USDGETS10XRPBOOKDIR}, MAXSEQ, _))
        .WillByDefault(Return(CreateOwnerDirLedgerObject(indexes, INDEX1).getSerializer().peekData()));

    // for reverse
    auto const indexes2 = std::vector<ripple::uint256>(10, ripple::uint256{INDEX1});
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::uint256{PAYS20XRPGETS10USDBOOKDIR}, MAXSEQ, _))
        .WillByDefault(Return(CreateOwnerDirLedgerObject(indexes2, INDEX2).getSerializer().peekData()));

    // offer owner account root
    ON_CALL(
        *rawBackendPtr, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT2)).key, MAXSEQ, _))
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT2, 0, 2, 200, 2, INDEX1, 2).getSerializer().peekData()));

    // issuer account root
    ON_CALL(
        *rawBackendPtr, doFetchLedgerObject(ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key, MAXSEQ, _))
        .WillByDefault(Return(CreateAccountRootObject(ACCOUNT, 0, 2, 200, 2, INDEX1, 2).getSerializer().peekData()));

    // fee
    auto feeBlob = CreateFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ripple::keylet::fees().key, MAXSEQ, _)).WillByDefault(Return(feeBlob));

    auto const gets10XRPPays20USDOffer = CreateOfferLedgerObject(
        ACCOUNT2,
        10,
        20,
        ripple::to_string(ripple::xrpCurrency()),
        ripple::to_string(ripple::to_currency("USD")),
        toBase58(ripple::xrpAccount()),
        ACCOUNT,
        PAYS20USDGETS10XRPBOOKDIR);

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
        PAYS20XRPGETS10USDBOOKDIR);

    std::vector<Blob> bbs(10, gets10XRPPays20USDOffer.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects(indexes, MAXSEQ, _)).WillByDefault(Return(bbs));

    // for reverse
    std::vector<Blob> bbs2(10, gets10USDPays20XRPOffer.getSerializer().peekData());
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects(indexes2, MAXSEQ, _)).WillByDefault(Return(bbs2));

    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(2);

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
        ACCOUNT);
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
        ACCOUNT);
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{SubscribeHandler{mockBackendPtr, subManager_}};
        auto const output = handler.process(input, Context{std::ref(yield), session_});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("offers").as_array().size(), 20);
        EXPECT_EQ(output->as_object().at("offers").as_array()[0].as_object(), json::parse(expectedOffer));
        EXPECT_EQ(output->as_object().at("offers").as_array()[10].as_object(), json::parse(expectedReversedOffer));
        std::this_thread::sleep_for(20ms);
        auto const report = subManager_->report();
        // original book + reverse book
        EXPECT_EQ(report.at("books").as_uint64(), 2);
    });
}
