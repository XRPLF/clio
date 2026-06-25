#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/Subscribe.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/MockWsBase.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/UintTypes.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
using namespace data;
using namespace testing;
using std::chrono::milliseconds;

namespace {

constexpr auto kMinSeq = 10;
constexpr auto kMaxSeq = 30;
constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kAccount2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kPayS20UsdGetS10XrpBookDir =
    "43B83ADC452B85FCBADA6CAEAC5181C255A213630D58FFD455071AFD498D0000";
constexpr auto kPayS20XrpGetS10UsdBookDir =
    "7B1767D41DBCE79D9585CF9D0262A5FEC45E5206FF524F8B55071AFD498D0000";
constexpr auto kIndex1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr auto kIndex2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

}  // namespace

struct RPCSubscribeHandlerTest : HandlerBaseTest {
protected:
    web::SubscriptionContextPtr session_ = std::make_shared<MockSession>();
    MockSession* mockSession_ = dynamic_cast<MockSession*>(session_.get());
    StrictMockSubscriptionManagerSharedPtr mockSubscriptionManagerPtr_;
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr_;
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
            .testJson = R"JSON({"accounts": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accountsNotArray"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsItemNotString",
            .testJson = R"JSON({"accounts": [123]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts'sItemNotString"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsItemInvalidString",
            .testJson = R"JSON({"accounts": ["123"]})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts'sItemMalformed"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsEmptyArray",
            .testJson = R"JSON({"accounts": []})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsProposedNotArray",
            .testJson = R"JSON({"accounts_proposed": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts_proposedNotArray"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsProposedItemNotString",
            .testJson = R"JSON({"accounts_proposed": [123]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "accounts_proposed'sItemNotString"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsProposedItemInvalidString",
            .testJson = R"JSON({"accounts_proposed": ["123"]})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts_proposed'sItemMalformed"
        },
        SubscribeParamTestCaseBundle{
            .testName = "AccountsProposedEmptyArray",
            .testJson = R"JSON({"accounts_proposed": []})JSON",
            .expectedError = "actMalformed",
            .expectedErrorMessage = "accounts_proposed malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamsNotArray",
            .testJson = R"JSON({"streams": 1})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "streamsNotArray"
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamNotString",
            .testJson = R"JSON({"streams": [1]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "streamNotString"
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamNotValid",
            .testJson = R"JSON({"streams": ["1"]})JSON",
            .expectedError = "malformedStream",
            .expectedErrorMessage = "Stream malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamPeerStatusNotSupport",
            .testJson = R"JSON({"streams": ["peer_status"]})JSON",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamConsensusNotSupport",
            .testJson = R"JSON({"streams": ["consensus"]})JSON",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
        SubscribeParamTestCaseBundle{
            .testName = "StreamServerNotSupport",
            .testJson = R"JSON({"streams": ["server"]})JSON",
            .expectedError = "notSupported",
            .expectedErrorMessage = "Operation not supported."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksNotArray",
            .testJson = R"JSON({"books": "1"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "booksNotArray"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemNotObject",
            .testJson = R"JSON({"books": ["1"]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "booksItemNotObject"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemMissingTakerPays",
            .testJson = R"JSON({"books": [{"taker_gets": {"currency": "XRP"}}]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Missing field 'taker_pays'"
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemMissingTakerGets",
            .testJson = R"JSON({"books": [{"taker_pays": {"currency": "XRP"}}]})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Missing field 'taker_gets'"
        },
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
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
            .expectedErrorMessage =
                "Unneeded field 'taker_gets.issuer' for XRP currency specification."
        },
        SubscribeParamTestCaseBundle{
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
            .expectedErrorMessage =
                "Unneeded field 'taker_pays.issuer' for XRP currency specification."
        },
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemInvalidSnapshot",
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
                        "snapshot": 0
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "snapshotNotBool"
        },
        SubscribeParamTestCaseBundle{
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
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemInvalidTakerNotString",
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
                        "taker": 0
                    }
                ]
            })JSON",
            .expectedError = "badIssuer",
            .expectedErrorMessage = "Issuer account malformed."
        },
        SubscribeParamTestCaseBundle{
            .testName = "BooksItemInvalidTaker",
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
                        "taker": "xxxxxxx"
                    }
                ]
            })JSON",
            .expectedError = "badIssuer",
            .expectedErrorMessage = "Issuer account malformed."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCSubscribe,
    SubscribeParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(SubscribeParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };
        auto const req = boost::json::parse(testBundle.testJson);
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
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output =
            handler.process(boost::json::parse(R"JSON({})JSON"), Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, StreamsWithoutLedger)
{
    // these streams don't return response
    auto const input = boost::json::parse(
        R"JSON({
            "streams": ["transactions_proposed", "transactions", "validations", "manifests", "book_changes"]
        })JSON"
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };
        EXPECT_CALL(*mockSubscriptionManagerPtr_, subTransactions);
        EXPECT_CALL(*mockSubscriptionManagerPtr_, subValidation);
        EXPECT_CALL(*mockSubscriptionManagerPtr_, subManifest);
        EXPECT_CALL(*mockSubscriptionManagerPtr_, subBookChanges);
        EXPECT_CALL(*mockSubscriptionManagerPtr_, subProposedTransactions);

        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, StreamsLedger)
{
    static constexpr auto kExpectedOutput =
        R"JSON({
            "validated_ledgers": "10-30",
            "ledger_index": 30,
            "ledger_hash": "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
            "ledger_time": 0,
            "fee_base": 1,
            "reserve_base": 3,
            "reserve_inc": 2
        })JSON";

    auto const input = boost::json::parse(
        R"JSON({
            "streams": ["ledger"]
        })JSON"
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };

        EXPECT_CALL(*mockSubscriptionManagerPtr_, subLedger)
            .WillOnce(testing::Return(boost::json::parse(kExpectedOutput).as_object()));

        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object(), boost::json::parse(kExpectedOutput));
    });
}

TEST_F(RPCSubscribeHandlerTest, Accounts)
{
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "accounts": ["{}", "{}", "{}"]
            }})JSON",
            kAccount,
            kAccount2,
            kAccount2
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };

        EXPECT_CALL(
            *mockSubscriptionManagerPtr_, subAccount(getAccountIdWithString(kAccount), session_)
        );
        EXPECT_CALL(
            *mockSubscriptionManagerPtr_, subAccount(getAccountIdWithString(kAccount2), session_)
        )
            .Times(2);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, AccountsProposed)
{
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "accounts_proposed": ["{}", "{}", "{}"]
            }})JSON",
            kAccount,
            kAccount2,
            kAccount2
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };

        EXPECT_CALL(
            *mockSubscriptionManagerPtr_,
            subProposedAccount(getAccountIdWithString(kAccount), session_)
        );
        EXPECT_CALL(
            *mockSubscriptionManagerPtr_,
            subProposedAccount(getAccountIdWithString(kAccount2), session_)
        )
            .Times(2);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, JustBooks)
{
    auto const input = boost::json::parse(
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
            kAccount
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };
        EXPECT_CALL(*mockSubscriptionManagerPtr_, subBook);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, BooksBothSet)
{
    auto const input = boost::json::parse(
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
            kAccount
        )
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };
        EXPECT_CALL(*mockSubscriptionManagerPtr_, subBook).Times(2);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output.result->as_object().empty());
    });
}

TEST_F(RPCSubscribeHandlerTest, BooksBothSnapshotSet)
{
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "books": [
                    {{
                        "taker_gets": {{
                            "currency": "XRP"
                        }},
                        "taker_pays": {{
                            "currency": "USD",
                            "issuer": "{}"
                        }},
                        "both": true,
                        "snapshot": true
                    }}
                ]
            }})JSON",
            kAccount
        )
    );
    backend_->setRange(kMinSeq, kMaxSeq);

    auto const issuer = getAccountIdWithString(kAccount);

    auto const getsXRPPaysUSDBook = getBookBase(
        rpc::parseBook(
            xrpl::toCurrency("USD"), issuer, xrpl::xrpCurrency(), xrpl::xrpAccount(), std::nullopt
        )
            .value()
    );

    auto const reversedBook = getBookBase(
        rpc::parseBook(
            xrpl::xrpCurrency(), xrpl::xrpAccount(), xrpl::toCurrency("USD"), issuer, std::nullopt
        )
            .value()
    );

    ON_CALL(*backend_, doFetchSuccessorKey(getsXRPPaysUSDBook, kMaxSeq, _))
        .WillByDefault(Return(xrpl::uint256{kPayS20UsdGetS10XrpBookDir}));

    ON_CALL(*backend_, doFetchSuccessorKey(xrpl::uint256{kPayS20UsdGetS10XrpBookDir}, kMaxSeq, _))
        .WillByDefault(Return(std::nullopt));

    ON_CALL(*backend_, doFetchSuccessorKey(reversedBook, kMaxSeq, _))
        .WillByDefault(Return(xrpl::uint256{kPayS20XrpGetS10UsdBookDir}));

    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(4);

    // 2 book dirs + 2 issuer global freeze + 2 transferRate + 1 owner root + 1 fee
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(8);

    auto const indexes = std::vector<xrpl::uint256>(10, xrpl::uint256{kIndex2});
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kPayS20UsdGetS10XrpBookDir}, kMaxSeq, _))
        .WillByDefault(
            Return(createOwnerDirLedgerObject(indexes, kIndex1).getSerializer().peekData())
        );

    // for reverse
    auto const indexes2 = std::vector<xrpl::uint256>(10, xrpl::uint256{kIndex1});
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kPayS20XrpGetS10UsdBookDir}, kMaxSeq, _))
        .WillByDefault(
            Return(createOwnerDirLedgerObject(indexes2, kIndex2).getSerializer().peekData())
        );

    // offer owner account root
    ON_CALL(
        *backend_,
        doFetchLedgerObject(
            xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, kMaxSeq, _
        )
    )
        .WillByDefault(Return(
            createAccountRootObject(kAccount2, 0, 2, 200, 2, kIndex1, 2).getSerializer().peekData()
        ));

    // issuer account root
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, kMaxSeq, _)
    )
        .WillByDefault(Return(
            createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2).getSerializer().peekData()
        ));

    // fee
    auto feeBlob = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kMaxSeq, _))
        .WillByDefault(Return(feeBlob));

    auto const gets10XRPPays20USDOffer = createOfferLedgerObject(
        kAccount2,
        10,
        20,
        xrpl::to_string(xrpl::xrpCurrency()),
        xrpl::to_string(xrpl::toCurrency("USD")),
        toBase58(xrpl::xrpAccount()),
        kAccount,
        kPayS20UsdGetS10XrpBookDir
    );

    // for reverse
    // offer owner is USD issuer
    auto const gets10USDPays20XRPOffer = createOfferLedgerObject(
        kAccount,
        10,
        20,
        xrpl::to_string(xrpl::toCurrency("USD")),
        xrpl::to_string(xrpl::xrpCurrency()),
        kAccount,
        toBase58(xrpl::xrpAccount()),
        kPayS20XrpGetS10UsdBookDir
    );

    std::vector<Blob> const bbs(10, gets10XRPPays20USDOffer.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects(indexes, kMaxSeq, _)).WillByDefault(Return(bbs));

    // for reverse
    std::vector<Blob> const bbs2(10, gets10USDPays20XRPOffer.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects(indexes2, kMaxSeq, _)).WillByDefault(Return(bbs2));

    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(2);

    static auto const kExpectedOffer = fmt::format(
        R"JSON({{
            "Account": "{}",
            "BookDirectory": "{}",
            "BookNode": "0",
            "Flags": 0,
            "LedgerEntryType": "Offer",
            "OwnerNode": "0",
            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
            "PreviousTxnLgrSeq": 0,
            "Sequence": 0,
            "TakerGets": "10",
            "TakerPays": {{
                "currency": "USD",
                "issuer": "{}",
                "value": "20"
            }},
            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
            "owner_funds": "193",
            "quality": "2"
        }})JSON",
        kAccount2,
        kPayS20UsdGetS10XrpBookDir,
        kAccount
    );
    static auto const kExpectedReversedOffer = fmt::format(
        R"JSON({{
            "Account": "{}",
            "BookDirectory": "{}",
            "BookNode": "0",
            "Flags": 0,
            "LedgerEntryType": "Offer",
            "OwnerNode": "0",
            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
            "PreviousTxnLgrSeq": 0,
            "Sequence": 0,
            "TakerGets": {{
                "currency": "USD",
                "issuer": "{}",
                "value": "10"
            }},
            "TakerPays": "20",
            "index": "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC",
            "owner_funds": "10",
            "quality": "2"
        }})JSON",
        kAccount,
        kPayS20XrpGetS10UsdBookDir,
        kAccount
    );
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };
        EXPECT_CALL(*mockSubscriptionManagerPtr_, subBook).Times(2);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("bids").as_array().size(), 10);
        EXPECT_EQ(output.result->as_object().at("asks").as_array().size(), 10);
        EXPECT_EQ(
            output.result->as_object().at("bids").as_array()[0].as_object(),
            boost::json::parse(kExpectedOffer)
        );
        EXPECT_EQ(
            output.result->as_object().at("asks").as_array()[0].as_object(),
            boost::json::parse(kExpectedReversedOffer)
        );
    });
}

TEST_F(RPCSubscribeHandlerTest, BooksBothUnsetSnapshotSet)
{
    auto const input = boost::json::parse(
        fmt::format(
            R"JSON({{
                "books": [
                    {{
                        "taker_gets": {{
                            "currency": "XRP"
                        }},
                        "taker_pays": {{
                            "currency": "USD",
                            "issuer": "{}"
                        }},
                        "snapshot": true
                    }}
                ]
            }})JSON",
            kAccount
        )
    );
    backend_->setRange(kMinSeq, kMaxSeq);

    auto const issuer = getAccountIdWithString(kAccount);

    auto const getsXRPPaysUSDBook = getBookBase(
        rpc::parseBook(
            xrpl::toCurrency("USD"), issuer, xrpl::xrpCurrency(), xrpl::xrpAccount(), std::nullopt
        )
            .value()
    );

    auto const reversedBook = getBookBase(
        rpc::parseBook(
            xrpl::xrpCurrency(), xrpl::xrpAccount(), xrpl::toCurrency("USD"), issuer, std::nullopt
        )
            .value()
    );

    ON_CALL(*backend_, doFetchSuccessorKey(getsXRPPaysUSDBook, kMaxSeq, _))
        .WillByDefault(Return(xrpl::uint256{kPayS20UsdGetS10XrpBookDir}));

    ON_CALL(*backend_, doFetchSuccessorKey(xrpl::uint256{kPayS20UsdGetS10XrpBookDir}, kMaxSeq, _))
        .WillByDefault(Return(std::nullopt));

    ON_CALL(*backend_, doFetchSuccessorKey(reversedBook, kMaxSeq, _))
        .WillByDefault(Return(xrpl::uint256{kPayS20XrpGetS10UsdBookDir}));

    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(2);

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(5);

    auto const indexes = std::vector<xrpl::uint256>(10, xrpl::uint256{kIndex2});
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kPayS20UsdGetS10XrpBookDir}, kMaxSeq, _))
        .WillByDefault(
            Return(createOwnerDirLedgerObject(indexes, kIndex1).getSerializer().peekData())
        );

    // for reverse
    auto const indexes2 = std::vector<xrpl::uint256>(10, xrpl::uint256{kIndex1});
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::uint256{kPayS20XrpGetS10UsdBookDir}, kMaxSeq, _))
        .WillByDefault(
            Return(createOwnerDirLedgerObject(indexes2, kIndex2).getSerializer().peekData())
        );

    // offer owner account root
    ON_CALL(
        *backend_,
        doFetchLedgerObject(
            xrpl::keylet::account(getAccountIdWithString(kAccount2)).key, kMaxSeq, _
        )
    )
        .WillByDefault(Return(
            createAccountRootObject(kAccount2, 0, 2, 200, 2, kIndex1, 2).getSerializer().peekData()
        ));

    // issuer account root
    ON_CALL(
        *backend_,
        doFetchLedgerObject(xrpl::keylet::account(getAccountIdWithString(kAccount)).key, kMaxSeq, _)
    )
        .WillByDefault(Return(
            createAccountRootObject(kAccount, 0, 2, 200, 2, kIndex1, 2).getSerializer().peekData()
        ));

    // fee
    auto feeBlob = createLegacyFeeSettingBlob(1, 2, 3, 4, 0);
    ON_CALL(*backend_, doFetchLedgerObject(xrpl::keylet::fees().key, kMaxSeq, _))
        .WillByDefault(Return(feeBlob));

    auto const gets10XRPPays20USDOffer = createOfferLedgerObject(
        kAccount2,
        10,
        20,
        xrpl::to_string(xrpl::xrpCurrency()),
        xrpl::to_string(xrpl::toCurrency("USD")),
        toBase58(xrpl::xrpAccount()),
        kAccount,
        kPayS20UsdGetS10XrpBookDir
    );

    // for reverse
    // offer owner is USD issuer
    auto const gets10USDPays20XRPOffer = createOfferLedgerObject(
        kAccount,
        10,
        20,
        xrpl::to_string(xrpl::toCurrency("USD")),
        xrpl::to_string(xrpl::xrpCurrency()),
        kAccount,
        toBase58(xrpl::xrpAccount()),
        kPayS20XrpGetS10UsdBookDir
    );

    std::vector<Blob> const bbs(10, gets10XRPPays20USDOffer.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects(indexes, kMaxSeq, _)).WillByDefault(Return(bbs));

    // for reverse
    std::vector<Blob> const bbs2(10, gets10USDPays20XRPOffer.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects(indexes2, kMaxSeq, _)).WillByDefault(Return(bbs2));

    EXPECT_CALL(*backend_, doFetchLedgerObjects);

    static auto const kExpectedOffer = fmt::format(
        R"JSON({{
            "Account": "{}",
            "BookDirectory": "{}",
            "BookNode": "0",
            "Flags": 0,
            "LedgerEntryType": "Offer",
            "OwnerNode": "0",
            "PreviousTxnID": "0000000000000000000000000000000000000000000000000000000000000000",
            "PreviousTxnLgrSeq": 0,
            "Sequence": 0,
            "TakerGets": "10",
            "TakerPays": {{
                "currency": "USD",
                "issuer": "{}",
                "value": "20"
            }},
            "index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
            "owner_funds": "193",
            "quality": "2"
        }})JSON",
        kAccount2,
        kPayS20UsdGetS10XrpBookDir,
        kAccount
    );

    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };
        EXPECT_CALL(*mockSubscriptionManagerPtr_, subBook);
        EXPECT_CALL(*mockSession_, setApiSubversion(0));
        auto const output = handler.process(input, Context{yield, session_});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->as_object().at("offers").as_array().size(), 10);
        EXPECT_EQ(
            output.result->as_object().at("offers").as_array()[0].as_object(),
            boost::json::parse(kExpectedOffer)
        );
    });
}

TEST_F(RPCSubscribeHandlerTest, APIVersion)
{
    auto const input = boost::json::parse(
        R"JSON({
            "streams": ["transactions_proposed"]
        })JSON"
    );
    auto const apiVersion = 2;
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{
            SubscribeHandler{backend_, mockAmendmentCenterPtr_, mockSubscriptionManagerPtr_}
        };
        EXPECT_CALL(*mockSubscriptionManagerPtr_, subProposedTransactions);
        EXPECT_CALL(*mockSession_, setApiSubversion(apiVersion));
        auto const output = handler.process(
            input, Context{.yield = yield, .session = session_, .apiVersion = apiVersion}
        );
        ASSERT_TRUE(output);
        // EXPECT_EQ(session_->apiSubVersion, apiVersion);
    });
}

TEST(RPCSubscribeHandlerSpecTest, DeprecatedFields)
{
    boost::json::value const json{
        {"streams", kAccount},
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
    EXPECT_EQ(obj.at("id").as_int64(), static_cast<int64_t>(WarningCode::WarnRpcDeprecated));
    auto const& message = obj.at("message").as_string();
    for (auto const& field : {"user", "password", "rt_accounts"}) {
        EXPECT_NE(message.find(fmt::format("Field '{}' is deprecated", field)), std::string::npos)
            << message;
    }
}
