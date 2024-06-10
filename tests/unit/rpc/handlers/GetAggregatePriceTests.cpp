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
#include "util/Fixtures.hpp"
#include "util/MockBackend.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/basics/Blob.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/UintTypes.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

constexpr static auto RANGEMIN = 10;
constexpr static auto RANGEMAX = 30;
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto TX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr static auto TX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr static auto INDEX = "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";

namespace {

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
    auto oracleObject = CreateOracleObject(
        account,
        "70726F7669646572",
        64u,
        time,
        ripple::Blob(8, 'a'),
        ripple::Blob(8, 'a'),
        RANGEMAX - 4,
        ripple::uint256{tx},
        CreatePriceDataSeries(
            {CreateOraclePriceData(price, ripple::to_currency("USD"), ripple::to_currency("XRP"), scale)}
        )
    );

    auto const oracleIndex = ripple::keylet::oracle(GetAccountIDWithString(account), docId).key;
    EXPECT_CALL(backend, doFetchLedgerObject(oracleIndex, RANGEMAX, _))
        .WillOnce(Return(oracleObject.getSerializer().peekData()));
}
};  // namespace

class RPCGetAggregatePriceHandlerTest : public HandlerBaseTest {
protected:
    void
    SetUp() override
    {
        HandlerBaseTest::SetUp();
        backend->setRange(RANGEMIN, RANGEMAX);
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
            "ledger_indexInvalid", R"({"ledger_index": "x"})", "invalidParams", "ledgerIndexMalformed"
        },
        GetAggregatePriceParamTestCaseBundle{
            "ledger_hashInvalid", R"({"ledger_hash": "x"})", "invalidParams", "ledger_hashMalformed"
        },
        GetAggregatePriceParamTestCaseBundle{
            "ledger_hashNotString", R"({"ledger_hash": 123})", "invalidParams", "ledger_hashNotString"
        },
        GetAggregatePriceParamTestCaseBundle{
            "no_oracles",
            R"({
                    "base_asset": "XRP",
                    "quote_asset": "USD"
                })",
            "invalidParams",
            "Required field 'oracles' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            "no_base_asset",
            R"({
                    "quote_asset": "USD",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            "invalidParams",
            "Required field 'base_asset' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            "no_quote_asset",
            R"({
                    "base_asset": "USD",
                    "oracles": 
                    [
                        {
                            "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                            "oracle_document_id": 2
                        }
                    ]
                })",
            "invalidParams",
            "Required field 'quote_asset' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            "invalid_quote_asset",
            R"({
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
            "invalidParams",
            "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            "invalid_quote_asset2",
            R"({
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
            "invalidParams",
            "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            "oraclesIsEmpty",
            R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": []
                })",
            "oracleMalformed",
            "Oracle request is malformed."
        },
        GetAggregatePriceParamTestCaseBundle{
            "oraclesNotArray",
            R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": 1
                })",
            "oracleMalformed",
            "Oracle request is malformed."
        },
        GetAggregatePriceParamTestCaseBundle{
            "thresholdNotInt",
            R"({
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
            "invalidParams",
            "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            "trimNotInt",
            R"({
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
            "invalidParams",
            "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            "trimTooSmall",
            R"({
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
            "invalidParams",
            "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            "trimTooLarge",
            R"({
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
            "invalidParams",
            "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            "oracleAccountInvalid",
            R"({
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
            "invalidParams",
            "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            "oracleDocumentIdNotInt",
            R"({
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
            "invalidParams",
            "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            "oracleMissingAccount",
            R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": [{"oracle_document_id": 2}]
                })",
            "oracleMalformed",
            "Oracle request is malformed."
        },
        GetAggregatePriceParamTestCaseBundle{
            "oracleMissingDocumentId",
            R"({
                    "base_asset": "USD",
                    "quote_asset": "XRP",
                    "oracles": [{"account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD"}]
                })",
            "oracleMalformed",
            "Oracle request is malformed."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCGetAggregatePriceGroup1,
    GetAggregatePriceParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(GetAggregatePriceParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillOnce(Return(std::nullopt));
    auto constexpr documentId = 1;
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
        ACCOUNT,
        documentId
    ));
    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId = 1;
    mockLedgerObject(*backend, ACCOUNT, documentId, TX1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId
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
        RANGEMAX,
        LEDGERHASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, PreviousTxNotFound)
{
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId = 1;
    mockLedgerObject(*backend, ACCOUNT, documentId, TX1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId
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
        RANGEMAX,
        LEDGERHASH
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
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId = 1;
    mockLedgerObject(*backend, ACCOUNT, documentId, TX1, 1e3, 2);  // 10

    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256(TX1), _))
        .WillRepeatedly(Return(CreateOracleSetTxWithMetadata(
            ACCOUNT,
            RANGEMAX,
            123,
            1,
            4321u,
            CreatePriceDataSeries({CreateOraclePriceData(1e3, ripple::to_currency("EUR"), ripple::to_currency("XRP"), 2)
            }),
            INDEX,
            true,
            TX2
        )));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId
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
        RANGEMAX,
        LEDGERHASH
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
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId1 = 1;
    auto constexpr documentId2 = 2;
    auto constexpr documentId3 = 3;
    mockLedgerObject(*backend, ACCOUNT, documentId1, TX1, 1e3, 2);  // 10
    mockLedgerObject(*backend, ACCOUNT, documentId2, TX1, 2e3, 2);  // 20
    mockLedgerObject(*backend, ACCOUNT, documentId3, TX1, 3e3, 1);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId1,
        ACCOUNT,
        documentId2,
        ACCOUNT,
        documentId3
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
        RANGEMAX,
        LEDGERHASH
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
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId1 = 1;
    auto constexpr documentId2 = 2;
    auto constexpr documentId3 = 3;
    auto constexpr documentId4 = 4;
    mockLedgerObject(*backend, ACCOUNT, documentId1, TX1, 1e3, 2);  // 10
    mockLedgerObject(*backend, ACCOUNT, documentId2, TX1, 2e3, 2);  // 20
    mockLedgerObject(*backend, ACCOUNT, documentId4, TX1, 4e2, 1);  // 40
    mockLedgerObject(*backend, ACCOUNT, documentId3, TX1, 3e3, 1);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId1,
        ACCOUNT,
        documentId2,
        ACCOUNT,
        documentId3,
        ACCOUNT,
        documentId4
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
        RANGEMAX,
        LEDGERHASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, OracleLedgerEntryTrim)
{
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    // prepare 4 prices, when trim is 25, the lowest(documentId1) and highest(documentId3) price will be removed
    auto constexpr documentId1 = 1;
    auto constexpr documentId2 = 2;
    auto constexpr documentId3 = 3;
    auto constexpr documentId4 = 4;
    mockLedgerObject(*backend, ACCOUNT, documentId1, TX1, 1e3, 2);  // 10
    mockLedgerObject(*backend, ACCOUNT, documentId2, TX1, 2e3, 2);  // 20
    mockLedgerObject(*backend, ACCOUNT, documentId4, TX1, 4e2, 1);  // 40
    mockLedgerObject(*backend, ACCOUNT, documentId3, TX1, 3e3, 1);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId1,
        ACCOUNT,
        documentId2,
        ACCOUNT,
        documentId3,
        ACCOUNT,
        documentId4
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
        RANGEMAX,
        LEDGERHASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, NoOracleEntryFound)
{
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId = 1;
    auto const oracleIndex = ripple::keylet::oracle(GetAccountIDWithString(ACCOUNT), documentId).key;
    EXPECT_CALL(*backend, doFetchLedgerObject(oracleIndex, RANGEMAX, _)).WillOnce(Return(std::nullopt));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId
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
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId = 1;
    mockLedgerObject(*backend, ACCOUNT, documentId, TX1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId
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
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId1 = 1;
    auto constexpr documentId2 = 2;
    auto constexpr documentId3 = 3;
    auto constexpr documentId4 = 4;
    auto constexpr timestamp1 = 1711461384u;
    auto constexpr timestamp2 = 1711461383u;
    auto constexpr timestamp3 = 1711461382u;
    auto constexpr timestamp4 = 1711461381u;
    mockLedgerObject(*backend, ACCOUNT, documentId1, TX1, 1e3, 2, timestamp1);  // 10
    mockLedgerObject(*backend, ACCOUNT, documentId2, TX1, 2e3, 2, timestamp2);  // 20
    mockLedgerObject(*backend, ACCOUNT, documentId4, TX1, 4e2, 1, timestamp3);  // 40
    mockLedgerObject(*backend, ACCOUNT, documentId3, TX1, 3e3, 1, timestamp4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId1,
        ACCOUNT,
        documentId2,
        ACCOUNT,
        documentId3,
        ACCOUNT,
        documentId4
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
        timestamp1,
        RANGEMAX,
        LEDGERHASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, ValidTimeThreshold)
{
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId1 = 1;
    auto constexpr documentId2 = 2;
    auto constexpr documentId3 = 3;
    auto constexpr documentId4 = 4;
    auto constexpr timestamp1 = 1711461384u;
    auto constexpr timestamp2 = 1711461383u;
    auto constexpr timestamp3 = 1711461382u;
    auto constexpr timestamp4 = 1711461381u;
    mockLedgerObject(*backend, ACCOUNT, documentId1, TX1, 1e3, 2, timestamp1);  // 10
    mockLedgerObject(*backend, ACCOUNT, documentId2, TX1, 2e3, 2, timestamp2);  // 20
    mockLedgerObject(*backend, ACCOUNT, documentId4, TX1, 4e2, 1, timestamp3);  // 40
    mockLedgerObject(*backend, ACCOUNT, documentId3, TX1, 3e3, 1, timestamp4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        timestamp1 - timestamp2,
        ACCOUNT,
        documentId1,
        ACCOUNT,
        documentId2,
        ACCOUNT,
        documentId3,
        ACCOUNT,
        documentId4
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
        timestamp1,
        RANGEMAX,
        LEDGERHASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, TimeThresholdTooLong)
{
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId1 = 1;
    auto constexpr documentId2 = 2;
    auto constexpr documentId3 = 3;
    auto constexpr documentId4 = 4;
    auto constexpr timestamp1 = 1711461384u;
    auto constexpr timestamp2 = 1711461383u;
    auto constexpr timestamp3 = 1711461382u;
    auto constexpr timestamp4 = 1711461381u;
    mockLedgerObject(*backend, ACCOUNT, documentId1, TX1, 1e3, 2, timestamp1);  // 10
    mockLedgerObject(*backend, ACCOUNT, documentId2, TX1, 2e3, 2, timestamp2);  // 20
    mockLedgerObject(*backend, ACCOUNT, documentId4, TX1, 4e2, 1, timestamp3);  // 40
    mockLedgerObject(*backend, ACCOUNT, documentId3, TX1, 3e3, 1, timestamp4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        timestamp1 + 1,
        ACCOUNT,
        documentId1,
        ACCOUNT,
        documentId2,
        ACCOUNT,
        documentId3,
        ACCOUNT,
        documentId4
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
        RANGEMAX,
        LEDGERHASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, TimeThresholdIncludeOldest)
{
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId1 = 1;
    auto constexpr documentId2 = 2;
    auto constexpr documentId3 = 3;
    auto constexpr documentId4 = 4;
    auto constexpr timestamp1 = 1711461384u;
    auto constexpr timestamp2 = 1711461383u;
    auto constexpr timestamp3 = 1711461382u;
    auto constexpr timestamp4 = 1711461381u;
    mockLedgerObject(*backend, ACCOUNT, documentId1, TX1, 1e3, 2, timestamp1);  // 10
    mockLedgerObject(*backend, ACCOUNT, documentId2, TX1, 2e3, 2, timestamp2);  // 20
    mockLedgerObject(*backend, ACCOUNT, documentId4, TX1, 4e2, 1, timestamp3);  // 40
    mockLedgerObject(*backend, ACCOUNT, documentId3, TX1, 3e3, 1, timestamp4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        timestamp4 - timestamp1,
        ACCOUNT,
        documentId1,
        ACCOUNT,
        documentId2,
        ACCOUNT,
        documentId3,
        ACCOUNT,
        documentId4
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
        RANGEMAX,
        LEDGERHASH
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
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId = 1;
    auto const oracleIndex = ripple::keylet::oracle(GetAccountIDWithString(ACCOUNT), documentId).key;
    mockLedgerObject(*backend, ACCOUNT, documentId, TX1, 1e3, 2);  // 10
    // return a tx which contains NewFields
    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256(TX1), _))
        .WillOnce(Return(CreateOracleSetTxWithMetadata(
            ACCOUNT,
            RANGEMAX,
            123,
            1,
            4321u,
            CreatePriceDataSeries({CreateOraclePriceData(1e3, ripple::to_currency("JPY"), ripple::to_currency("XRP"), 2)
            }),
            ripple::to_string(oracleIndex),
            false,
            TX1
        )));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId
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
        RANGEMAX,
        LEDGERHASH
    ));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}
TEST_F(RPCGetAggregatePriceHandlerTest, NotFoundInTxHistory)
{
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _))
        .WillOnce(Return(CreateLedgerHeader(LEDGERHASH, RANGEMAX)));

    auto constexpr documentId = 1;
    auto const oracleIndex = ripple::keylet::oracle(GetAccountIDWithString(ACCOUNT), documentId).key;
    mockLedgerObject(*backend, ACCOUNT, documentId, TX1, 1e3, 2);  // 10

    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256(TX1), _))
        .WillOnce(Return(CreateOracleSetTxWithMetadata(
            ACCOUNT,
            RANGEMAX,
            123,
            1,
            4321u,
            CreatePriceDataSeries({CreateOraclePriceData(1e3, ripple::to_currency("EUR"), ripple::to_currency("XRP"), 2)
            }),
            ripple::to_string(oracleIndex),
            false,
            TX2
        )));

    EXPECT_CALL(*backend, fetchTransaction(ripple::uint256(TX2), _))
        .WillRepeatedly(Return(CreateOracleSetTxWithMetadata(
            ACCOUNT,
            RANGEMAX,
            123,
            1,
            4321u,
            CreatePriceDataSeries({CreateOraclePriceData(1e3, ripple::to_currency("EUR"), ripple::to_currency("XRP"), 2)
            }),
            ripple::to_string(oracleIndex),
            false,
            TX2
        )));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
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
        ACCOUNT,
        documentId
    ));

    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "The requested object was not found.");
    });
}
