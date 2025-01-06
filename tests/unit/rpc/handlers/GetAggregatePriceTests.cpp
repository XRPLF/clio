//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/GetAggregatePrice.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockBackend.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kRANGE_MIN = 10;
constexpr auto kRANGE_MAX = 30;
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kTX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kTX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kINDEX = "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";

void
mockLedgerObject(
    MockBackend const& backend,
    char const* account,
    std::uint32_t docId,
    char const* tx,
    std::uint32_t price,
    std::uint32_t scale,
    std::uint32_t time = 4321u
)
{
    auto oracleObject = createOracleObject(
        account,
        "70726F7669646572",
        64u,
        time,
        ripple::Blob(8, 'a'),
        ripple::Blob(8, 'a'),
        kRANGE_MAX - 4,
        ripple::uint256{tx},
        createPriceDataSeries(
            {createOraclePriceData(price, ripple::to_currency("USD"), ripple::to_currency("XRP"), scale)}
        )
    );

    auto const oracleIndex = ripple::keylet::oracle(getAccountIdWithString(account), docId).key;
    EXPECT_CALL(backend, doFetchLedgerObject(oracleIndex, kRANGE_MAX, _))
        .WillOnce(Return(oracleObject.getSerializer().peekData()));
}
};  // namespace

class RPCGetAggregatePriceHandlerTest : public HandlerBaseTest {
protected:
    void
    SetUp() override
    {
        HandlerBaseTest::SetUp();
        backend_->setRange(kRANGE_MIN, kRANGE_MAX);
    }
};

struct GetAggregatePriceParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct GetAggregatePriceParameterTest : public RPCGetAggregatePriceHandlerTest,
                                        public WithParamInterface<GetAggregatePriceParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<GetAggregatePriceParamTestCaseBundle>{
        GetAggregatePriceParamTestCaseBundle{
            .testName = "ledger_indexInvalid",
            .testJson = R"({"ledger_index": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "ledger_hashInvalid",
            .testJson = R"({"ledger_hash": "x"})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "ledger_hashNotString",
            .testJson = R"({"ledger_hash": 123})",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "no_oracles",
            .testJson = R"({
                    "base_asset": "XRP",
                    "quote_asset": "USD"
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'oracles' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "no_base_asset",
            .testJson = R"({
                    "quote_asset": "USD",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'base_asset' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "invalid_base_asset",
            .testJson = R"({
                    "quote_asset" : "USD",
                    "base_asset": "asdf",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "emtpy_base_asset",
            .testJson = R"({
                    "quote_asset" : "USD",
                    "base_asset": "",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "invalid_base_asset2",
            .testJson = R"({
                    "quote_asset" : "USD",
                    "base_asset": "+aa",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "no_quote_asset",
            .testJson = R"({
                    "base_asset": "USD",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'quote_asset' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "invalid_quote_asset",
            .testJson = R"({
                    "quote_asset" : "asdf",
                    "base_asset": "USD",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "empty_quote_asset",
            .testJson = R"({
                    "quote_asset" : "",
                    "base_asset": "USD",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "invalid_quote_asset2",
            .testJson = R"({
                    "quote_asset" : "+aa",
                    "base_asset": "USD",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oraclesIsEmpty",
            .testJson = R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": []
                })",
            .expectedError = "oracleMalformed",
            .expectedErrorMessage = "Oracle request is malformed."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oraclesNotArray",
            .testJson = R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": 1
                })",
            .expectedError = "oracleMalformed",
            .expectedErrorMessage = "Oracle request is malformed."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "thresholdNotInt",
            .testJson = R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ],
                    "time_threshold": "x"
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "trimNotInt",
            .testJson = R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ],
                    "trim": "x"
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "trimTooSmall",
            .testJson = R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles":
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ],
                    "trim": 0
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "trimTooLarge",
            .testJson = R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ],
                    "trim": 26
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oracleAccountInvalid",
            .testJson = R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": 
                    [
                        {
                            "account": "invalid",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oracleDocumentIdNotInt",
            .testJson = R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles":
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": "a"
                        }
                    ]
                })",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oracleMissingAccount",
            .testJson = R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": [{"oracle_document_id": 2}]
                })",
            .expectedError = "oracleMalformed",
            .expectedErrorMessage = "Oracle request is malformed."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oracleMissingDocumentId",
            .testJson = R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": [{"account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD"}]
                })",
            .expectedError = "oracleMalformed",
            .expectedErrorMessage = "Oracle request is malformed."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCGetAggregatePriceGroup1,
    GetAggregatePriceParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(GetAggregatePriceParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, OverOraclesMax)
{
    auto req = json::parse(
        R"({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": []
            })"
    );
    auto const maxOracles = 200;

    for (auto i = 0; i < maxOracles + 1; ++i) {
        req.at("oracles").as_array().push_back(
            json::object{{"account", "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD"}, {"oracle_document_id", 2}}
        );
    }
    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "oracleMalformed");
        EXPECT_EQ(err.at("error_message").as_string(), "Oracle request is malformed.");
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, LedgerNotFound)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _)).WillOnce(Return(std::nullopt));
    constexpr auto kDOCUMENT_ID = 1;
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID
    ));
    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, OracleLedgerEntrySinglePriceData)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID = 1;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID, kTX1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, OracleLedgerEntryStrOracleDocumentId)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID = 1;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID, kTX1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": "{}"
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, PreviousTxNotFound)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID = 1;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID, kTX1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "JPY",
                "quote_asset": "XRP",
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "The requested object was not found.");
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, NewLedgerObjectHasNoPricePair)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID = 1;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID, kTX1, 1e3, 2);  // 10

    EXPECT_CALL(*backend_, fetchTransaction(ripple::uint256(kTX1), _))
        .WillRepeatedly(Return(createOracleSetTxWithMetadata(
            kACCOUNT,
            kRANGE_MAX,
            123,
            1,
            4321u,
            createPriceDataSeries({createOraclePriceData(1e3, ripple::to_currency("EUR"), ripple::to_currency("XRP"), 2)
            }),
            kINDEX,
            true,
            kTX2
        )));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "JPY",
                "quote_asset": "XRP",
                "oracles":
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "The requested object was not found.");
    });
}

// median is the middle value of a set of numbers when there are odd number of price
TEST_F(RPCGetAggregatePriceHandlerTest, OracleLedgerEntryMultipleOraclesOdd)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID1 = 1;
    constexpr auto kDOCUMENT_ID2 = 2;
    constexpr auto kDOCUMENT_ID3 = 3;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID1, kTX1, 1e3, 2);  // 10
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID2, kTX1, 2e3, 2);  // 20
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID3, kTX1, 3e3, 1);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID1,
        kACCOUNT,
        kDOCUMENT_ID2,
        kACCOUNT,
        kDOCUMENT_ID3
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "110",
                    "size": 3,
                    "standard_deviation": "164.6207763315433"
                }},
                "median": "20",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

// median is the middle value of a set of numbers when there are odd number of price
TEST_F(RPCGetAggregatePriceHandlerTest, OracleLedgerEntryMultipleOraclesEven)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID1 = 1;
    constexpr auto kDOCUMENT_ID2 = 2;
    constexpr auto kDOCUMENT_ID3 = 3;
    constexpr auto kDOCUMENT_ID4 = 4;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID1, kTX1, 1e3, 2);  // 10
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID2, kTX1, 2e3, 2);  // 20
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID4, kTX1, 4e2, 1);  // 40
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID3, kTX1, 3e3, 1);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID1,
        kACCOUNT,
        kDOCUMENT_ID2,
        kACCOUNT,
        kDOCUMENT_ID3,
        kACCOUNT,
        kDOCUMENT_ID4
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "92.5",
                    "size": 4,
                    "standard_deviation": "138.8944443333378"
                }},
                "median": "30",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, OracleLedgerEntryTrim)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    // prepare 4 prices, when trim is 25, the lowest(documentId1) and highest(documentId3) price will be removed
    constexpr auto kDOCUMENT_ID1 = 1;
    constexpr auto kDOCUMENT_ID2 = 2;
    constexpr auto kDOCUMENT_ID3 = 3;
    constexpr auto kDOCUMENT_ID4 = 4;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID1, kTX1, 1e3, 2);  // 10
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID2, kTX1, 2e3, 2);  // 20
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID4, kTX1, 4e2, 1);  // 40
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID3, kTX1, 3e3, 1);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "trim": {},
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        25,
        kACCOUNT,
        kDOCUMENT_ID1,
        kACCOUNT,
        kDOCUMENT_ID2,
        kACCOUNT,
        kDOCUMENT_ID3,
        kACCOUNT,
        kDOCUMENT_ID4
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "92.5",
                    "size": 4,
                    "standard_deviation": "138.8944443333378"
                }},
                "trimmed_set": 
                {{
                    "mean": "30",
                    "size": 2,
                    "standard_deviation": "14.14213562373095"
                }},
                "median": "30",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, NoOracleEntryFound)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID = 1;
    auto const oracleIndex = ripple::keylet::oracle(getAccountIdWithString(kACCOUNT), kDOCUMENT_ID).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(oracleIndex, kRANGE_MAX, _)).WillOnce(Return(std::nullopt));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID
    ));

    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "The requested object was not found.");
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, NoMatchAssetPair)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID = 1;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID, kTX1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "JPY",
                "quote_asset": "XRP",
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID
    ));

    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "The requested object was not found.");
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, TimeThresholdIsZero)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID1 = 1;
    constexpr auto kDOCUMENT_ID2 = 2;
    constexpr auto kDOCUMENT_ID3 = 3;
    constexpr auto kDOCUMENT_ID4 = 4;
    constexpr auto kTIMESTAMP1 = 1711461384u;
    constexpr auto kTIMESTAMP2 = 1711461383u;
    constexpr auto kTIMESTAMP3 = 1711461382u;
    constexpr auto kTIMESTAMP4 = 1711461381u;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID1, kTX1, 1e3, 2, kTIMESTAMP1);  // 10
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID2, kTX1, 2e3, 2, kTIMESTAMP2);  // 20
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID4, kTX1, 4e2, 1, kTIMESTAMP3);  // 40
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID3, kTX1, 3e3, 1, kTIMESTAMP4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "time_threshold": {},
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        0,
        kACCOUNT,
        kDOCUMENT_ID1,
        kACCOUNT,
        kDOCUMENT_ID2,
        kACCOUNT,
        kDOCUMENT_ID3,
        kACCOUNT,
        kDOCUMENT_ID4
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": {},
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kTIMESTAMP1,
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, ValidTimeThreshold)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID1 = 1;
    constexpr auto kDOCUMENT_ID2 = 2;
    constexpr auto kDOCUMENT_ID3 = 3;
    constexpr auto kDOCUMENT_ID4 = 4;
    constexpr auto kTIMESTAMP1 = 1711461384u;
    constexpr auto kTIMESTAMP2 = 1711461383u;
    constexpr auto kTIMESTAMP3 = 1711461382u;
    constexpr auto kTIMESTAMP4 = 1711461381u;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID1, kTX1, 1e3, 2, kTIMESTAMP1);  // 10
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID2, kTX1, 2e3, 2, kTIMESTAMP2);  // 20
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID4, kTX1, 4e2, 1, kTIMESTAMP3);  // 40
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID3, kTX1, 3e3, 1, kTIMESTAMP4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "time_threshold": {},
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kTIMESTAMP1 - kTIMESTAMP2,
        kACCOUNT,
        kDOCUMENT_ID1,
        kACCOUNT,
        kDOCUMENT_ID2,
        kACCOUNT,
        kDOCUMENT_ID3,
        kACCOUNT,
        kDOCUMENT_ID4
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "15",
                    "size": 2,
                    "standard_deviation": "7.071067811865475"
                }},
                "median": "15",
                "time": {},
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kTIMESTAMP1,
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, TimeThresholdTooLong)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID1 = 1;
    constexpr auto kDOCUMENT_ID2 = 2;
    constexpr auto kDOCUMENT_ID3 = 3;
    constexpr auto kDOCUMENT_ID4 = 4;
    constexpr auto kTIMESTAMP1 = 1711461384u;
    constexpr auto kTIMESTAMP2 = 1711461383u;
    constexpr auto kTIMESTAMP3 = 1711461382u;
    constexpr auto kTIMESTAMP4 = 1711461381u;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID1, kTX1, 1e3, 2, kTIMESTAMP1);  // 10
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID2, kTX1, 2e3, 2, kTIMESTAMP2);  // 20
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID4, kTX1, 4e2, 1, kTIMESTAMP3);  // 40
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID3, kTX1, 3e3, 1, kTIMESTAMP4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "time_threshold": {},
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kTIMESTAMP1 + 1,
        kACCOUNT,
        kDOCUMENT_ID1,
        kACCOUNT,
        kDOCUMENT_ID2,
        kACCOUNT,
        kDOCUMENT_ID3,
        kACCOUNT,
        kDOCUMENT_ID4
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "92.5",
                    "size": 4,
                    "standard_deviation": "138.8944443333378"
                }},
                "median": "30",
                "time": 1711461384,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, TimeThresholdIncludeOldest)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID1 = 1;
    constexpr auto kDOCUMENT_ID2 = 2;
    constexpr auto kDOCUMENT_ID3 = 3;
    constexpr auto kDOCUMENT_ID4 = 4;
    constexpr auto kTIMESTAMP1 = 1711461384u;
    constexpr auto kTIMESTAMP2 = 1711461383u;
    constexpr auto kTIMESTAMP3 = 1711461382u;
    constexpr auto kTIMESTAMP4 = 1711461381u;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID1, kTX1, 1e3, 2, kTIMESTAMP1);  // 10
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID2, kTX1, 2e3, 2, kTIMESTAMP2);  // 20
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID4, kTX1, 4e2, 1, kTIMESTAMP3);  // 40
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID3, kTX1, 3e3, 1, kTIMESTAMP4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "time_threshold": {},
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }},
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kTIMESTAMP4 - kTIMESTAMP1,
        kACCOUNT,
        kDOCUMENT_ID1,
        kACCOUNT,
        kDOCUMENT_ID2,
        kACCOUNT,
        kDOCUMENT_ID3,
        kACCOUNT,
        kDOCUMENT_ID4
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "92.5",
                    "size": 4,
                    "standard_deviation": "138.8944443333378"
                }},
                "median": "30",
                "time": 1711461384,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

// When the price pair is not available in the current oracle, trace back to previous transactions
TEST_F(RPCGetAggregatePriceHandlerTest, FromTx)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID = 1;
    auto const oracleIndex = ripple::keylet::oracle(getAccountIdWithString(kACCOUNT), kDOCUMENT_ID).key;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID, kTX1, 1e3, 2);  // 10
    // return a tx which contains NewFields
    EXPECT_CALL(*backend_, fetchTransaction(ripple::uint256(kTX1), _))
        .WillOnce(Return(createOracleSetTxWithMetadata(
            kACCOUNT,
            kRANGE_MAX,
            123,
            1,
            4321u,
            createPriceDataSeries({createOraclePriceData(1e3, ripple::to_currency("JPY"), ripple::to_currency("XRP"), 2)
            }),
            ripple::to_string(oracleIndex),
            false,
            kTX1
        )));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "JPY",
                "quote_asset": "XRP",
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID
    ));

    auto const expected = json::parse(fmt::format(
        R"({{
                "entire_set": 
                {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})",
        kRANGE_MAX,
        kLEDGER_HASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}
TEST_F(RPCGetAggregatePriceHandlerTest, NotFoundInTxHistory)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRANGE_MAX, _))
        .WillOnce(Return(createLedgerHeader(kLEDGER_HASH, kRANGE_MAX)));

    constexpr auto kDOCUMENT_ID = 1;
    auto const oracleIndex = ripple::keylet::oracle(getAccountIdWithString(kACCOUNT), kDOCUMENT_ID).key;
    mockLedgerObject(*backend_, kACCOUNT, kDOCUMENT_ID, kTX1, 1e3, 2);  // 10

    EXPECT_CALL(*backend_, fetchTransaction(ripple::uint256(kTX1), _))
        .WillOnce(Return(createOracleSetTxWithMetadata(
            kACCOUNT,
            kRANGE_MAX,
            123,
            1,
            4321u,
            createPriceDataSeries({createOraclePriceData(1e3, ripple::to_currency("EUR"), ripple::to_currency("XRP"), 2)
            }),
            ripple::to_string(oracleIndex),
            false,
            kTX2
        )));

    EXPECT_CALL(*backend_, fetchTransaction(ripple::uint256(kTX2), _))
        .WillRepeatedly(Return(createOracleSetTxWithMetadata(
            kACCOUNT,
            kRANGE_MAX,
            123,
            1,
            4321u,
            createPriceDataSeries({createOraclePriceData(1e3, ripple::to_currency("EUR"), ripple::to_currency("XRP"), 2)
            }),
            ripple::to_string(oracleIndex),
            false,
            kTX2
        )));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = json::parse(fmt::format(
        R"({{
                "base_asset": "JPY",
                "quote_asset": "XRP",
                "oracles": 
                [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})",
        kACCOUNT,
        kDOCUMENT_ID
    ));

    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "The requested object was not found.");
    });
}
