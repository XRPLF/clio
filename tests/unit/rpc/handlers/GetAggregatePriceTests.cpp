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
#include <fmt/format.h>
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
using namespace data;
using namespace testing;

namespace {

constexpr auto kRangeMin = 10;
constexpr auto kRangeMax = 30;
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kAccount = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kTx1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kTx2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kIndex = "13F1A95D7AAB7108D5CE7EEAF504B2894B8C674E6D68499076441C4837282BF8";

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
        xrpl::Blob(8, 'a'),
        xrpl::Blob(8, 'a'),
        kRangeMax - 4,
        xrpl::uint256{tx},
        createPriceDataSeries(
            {createOraclePriceData(price, xrpl::toCurrency("USD"), xrpl::toCurrency("XRP"), scale)}
        )
    );

    auto const oracleIndex = xrpl::keylet::oracle(getAccountIdWithString(account), docId).key;
    EXPECT_CALL(backend, doFetchLedgerObject(oracleIndex, kRangeMax, _))
        .WillOnce(Return(oracleObject.getSerializer().peekData()));
}
};  // namespace

class RPCGetAggregatePriceHandlerTest : public HandlerBaseTest {
protected:
    RPCGetAggregatePriceHandlerTest()
    {
        backend_->setRange(kRangeMin, kRangeMax);
    }
};

struct GetAggregatePriceParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct GetAggregatePriceParameterTest
    : public RPCGetAggregatePriceHandlerTest,
      public WithParamInterface<GetAggregatePriceParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<GetAggregatePriceParamTestCaseBundle>{
        GetAggregatePriceParamTestCaseBundle{
            .testName = "ledger_indexInvalid",
            .testJson = R"JSON({"ledger_index": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledgerIndexMalformed"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "ledger_hashInvalid",
            .testJson = R"JSON({"ledger_hash": "x"})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashMalformed"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "ledger_hashNotString",
            .testJson = R"JSON({"ledger_hash": 123})JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "ledger_hashNotString"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "no_oracles",
            .testJson = R"JSON({
                "base_asset": "XRP",
                "quote_asset": "USD"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'oracles' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "no_base_asset",
            .testJson = R"JSON({
                "quote_asset": "USD",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'base_asset' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "invalid_base_asset",
            .testJson = R"JSON({
                "quote_asset": "USD",
                "base_asset": "asdf",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "empty_base_asset",
            .testJson = R"JSON({
                "quote_asset": "USD",
                "base_asset": "",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "invalid_base_asset2",
            .testJson = R"JSON({
                "quote_asset": "USD",
                "base_asset": "+aa",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "no_quote_asset",
            .testJson = R"JSON({
                "base_asset": "USD",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Required field 'quote_asset' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "invalid_quote_asset",
            .testJson = R"JSON({
                "quote_asset": "asdf",
                "base_asset": "USD",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "empty_quote_asset",
            .testJson = R"JSON({
                "quote_asset": "",
                "base_asset": "USD",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "invalid_quote_asset2",
            .testJson = R"JSON({
                "quote_asset": "+aa",
                "base_asset": "USD",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oraclesIsEmpty",
            .testJson = R"JSON({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": []
            })JSON",
            .expectedError = "oracleMalformed",
            .expectedErrorMessage = "Oracle request is malformed."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oraclesNotArray",
            .testJson = R"JSON({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": 1
            })JSON",
            .expectedError = "oracleMalformed",
            .expectedErrorMessage = "Oracle request is malformed."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "thresholdNotInt",
            .testJson = R"JSON({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ],
                "time_threshold": "x"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "trimNotInt",
            .testJson = R"JSON({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ],
                "trim": "x"
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "trimTooSmall",
            .testJson = R"JSON({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ],
                "trim": 0
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "trimTooLarge",
            .testJson = R"JSON({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": 2
                    }
                ],
                "trim": 26
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oracleAccountInvalid",
            .testJson = R"JSON({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
                    {
                        "account": "invalid",
                        "oracle_document_id": 2
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oracleDocumentIdNotInt",
            .testJson = R"JSON({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
                    {
                        "account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD",
                        "oracle_document_id": "a"
                    }
                ]
            })JSON",
            .expectedError = "invalidParams",
            .expectedErrorMessage = "Invalid parameters."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oracleMissingAccount",
            .testJson = R"JSON({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [{"oracle_document_id": 2}]
            })JSON",
            .expectedError = "oracleMalformed",
            .expectedErrorMessage = "Oracle request is malformed."
        },
        GetAggregatePriceParamTestCaseBundle{
            .testName = "oracleMissingDocumentId",
            .testJson = R"JSON({
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [{"account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD"}]
            })JSON",
            .expectedError = "oracleMalformed",
            .expectedErrorMessage = "Oracle request is malformed."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCGetAggregatePriceGroup1,
    GetAggregatePriceParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(GetAggregatePriceParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
        auto const req = boost::json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, OverOraclesMax)
{
    auto req = boost::json::parse(
        R"JSON({
            "base_asset": "USD",
            "quote_asset": "XRP",
            "oracles": []
        })JSON"
    );
    auto const maxOracles = 200;

    for (auto i = 0; i < maxOracles + 1; ++i) {
        req.at("oracles").as_array().push_back(
            boost::json::object{
                {"account", "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD"}, {"oracle_document_id", 2}
            }
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
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillOnce(Return(std::nullopt));
    constexpr auto kDocumentId = 1;
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})JSON",
            kAccount,
            kDocumentId
        )
    );
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
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentId = 1;
    mockLedgerObject(*backend_, kAccount, kDocumentId, kTx1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})JSON",
            kAccount,
            kDocumentId
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kRangeMax,
            kLedgerHash
        )
    );
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, OracleLedgerEntryStrOracleDocumentId)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentId = 1;
    mockLedgerObject(*backend_, kAccount, kDocumentId, kTx1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
                    {{
                        "account": "{}",
                        "oracle_document_id": "{}"
                    }}
                ]
            }})JSON",
            kAccount,
            kDocumentId
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kRangeMax,
            kLedgerHash
        )
    );
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, PreviousTxNotFound)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentId = 1;
    mockLedgerObject(*backend_, kAccount, kDocumentId, kTx1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "JPY",
                "quote_asset": "XRP",
                "oracles": [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})JSON",
            kAccount,
            kDocumentId
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kRangeMax,
            kLedgerHash
        )
    );
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
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentId = 1;
    mockLedgerObject(*backend_, kAccount, kDocumentId, kTx1, 1e3, 2);  // 10

    EXPECT_CALL(*backend_, fetchTransaction(xrpl::uint256(kTx1), _))
        .WillRepeatedly(Return(createOracleSetTxWithMetadata(
            kAccount,
            kRangeMax,
            123,
            1,
            4321u,
            createPriceDataSeries(
                {createOraclePriceData(1e3, xrpl::toCurrency("EUR"), xrpl::toCurrency("XRP"), 2)}
            ),
            kIndex,
            true,
            kTx2
        )));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "JPY",
                "quote_asset": "XRP",
                "oracles": [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})JSON",
            kAccount,
            kDocumentId
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kRangeMax,
            kLedgerHash
        )
    );
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
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentID1 = 1;
    constexpr auto kDocumentID2 = 2;
    constexpr auto kDocumentID3 = 3;
    mockLedgerObject(*backend_, kAccount, kDocumentID1, kTx1, 1e3, 2);  // 10
    mockLedgerObject(*backend_, kAccount, kDocumentID2, kTx1, 2e3, 2);  // 20
    mockLedgerObject(*backend_, kAccount, kDocumentID3, kTx1, 3e3, 1);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
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
            }})JSON",
            kAccount,
            kDocumentID1,
            kAccount,
            kDocumentID2,
            kAccount,
            kDocumentID3
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "110",
                    "size": 3,
                    "standard_deviation": "164.6207763315432795"
                }},
                "median": "20",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kRangeMax,
            kLedgerHash
        )
    );
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

// median is the middle value of a set of numbers when there are odd number of price
TEST_F(RPCGetAggregatePriceHandlerTest, OracleLedgerEntryMultipleOraclesEven)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentID1 = 1;
    constexpr auto kDocumentID2 = 2;
    constexpr auto kDocumentID3 = 3;
    constexpr auto kDocumentID4 = 4;
    mockLedgerObject(*backend_, kAccount, kDocumentID1, kTx1, 1e3, 2);  // 10
    mockLedgerObject(*backend_, kAccount, kDocumentID2, kTx1, 2e3, 2);  // 20
    mockLedgerObject(*backend_, kAccount, kDocumentID4, kTx1, 4e2, 1);  // 40
    mockLedgerObject(*backend_, kAccount, kDocumentID3, kTx1, 3e3, 1);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
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
            }})JSON",
            kAccount,
            kDocumentID1,
            kAccount,
            kDocumentID2,
            kAccount,
            kDocumentID3,
            kAccount,
            kDocumentID4
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "92.5",
                    "size": 4,
                    "standard_deviation": "138.8944443333377776"
                }},
                "median": "30",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kRangeMax,
            kLedgerHash
        )
    );
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, OracleLedgerEntryTrim)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    // prepare 4 prices, when trim is 25, the lowest(documentId1) and highest(documentId3) price
    // will be removed
    constexpr auto kDocumentID1 = 1;
    constexpr auto kDocumentID2 = 2;
    constexpr auto kDocumentID3 = 3;
    constexpr auto kDocumentID4 = 4;
    mockLedgerObject(*backend_, kAccount, kDocumentID1, kTx1, 1e3, 2);  // 10
    mockLedgerObject(*backend_, kAccount, kDocumentID2, kTx1, 2e3, 2);  // 20
    mockLedgerObject(*backend_, kAccount, kDocumentID4, kTx1, 4e2, 1);  // 40
    mockLedgerObject(*backend_, kAccount, kDocumentID3, kTx1, 3e3, 1);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "trim": {},
                "oracles": [
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
            }})JSON",
            25,
            kAccount,
            kDocumentID1,
            kAccount,
            kDocumentID2,
            kAccount,
            kDocumentID3,
            kAccount,
            kDocumentID4
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "92.5",
                    "size": 4,
                    "standard_deviation": "138.8944443333377776"
                }},
                "trimmed_set": {{
                    "mean": "30",
                    "size": 2,
                    "standard_deviation": "14.14213562373095049"
                }},
                "median": "30",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kRangeMax,
            kLedgerHash
        )
    );
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, NoOracleEntryFound)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentId = 1;
    auto const oracleIndex =
        xrpl::keylet::oracle(getAccountIdWithString(kAccount), kDocumentId).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject(oracleIndex, kRangeMax, _))
        .WillOnce(Return(std::nullopt));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "oracles": [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})JSON",
            kAccount,
            kDocumentId
        )
    );

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
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentId = 1;
    mockLedgerObject(*backend_, kAccount, kDocumentId, kTx1, 1e3, 2);  // 10

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "JPY",
                "quote_asset": "XRP",
                "oracles": [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})JSON",
            kAccount,
            kDocumentId
        )
    );

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
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentID1 = 1;
    constexpr auto kDocumentID2 = 2;
    constexpr auto kDocumentID3 = 3;
    constexpr auto kDocumentID4 = 4;
    constexpr auto kTimestamp1 = 1711461384u;
    constexpr auto kTimestamp2 = 1711461383u;
    constexpr auto kTimestamp3 = 1711461382u;
    constexpr auto kTimestamp4 = 1711461381u;
    mockLedgerObject(*backend_, kAccount, kDocumentID1, kTx1, 1e3, 2, kTimestamp1);  // 10
    mockLedgerObject(*backend_, kAccount, kDocumentID2, kTx1, 2e3, 2, kTimestamp2);  // 20
    mockLedgerObject(*backend_, kAccount, kDocumentID4, kTx1, 4e2, 1, kTimestamp3);  // 40
    mockLedgerObject(*backend_, kAccount, kDocumentID3, kTx1, 3e3, 1, kTimestamp4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "time_threshold": {},
                "oracles": [
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
            }})JSON",
            0,
            kAccount,
            kDocumentID1,
            kAccount,
            kDocumentID2,
            kAccount,
            kDocumentID3,
            kAccount,
            kDocumentID4
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": {},
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kTimestamp1,
            kRangeMax,
            kLedgerHash
        )
    );
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, ValidTimeThreshold)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentID1 = 1;
    constexpr auto kDocumentID2 = 2;
    constexpr auto kDocumentID3 = 3;
    constexpr auto kDocumentID4 = 4;
    constexpr auto kTimestamp1 = 1711461384u;
    constexpr auto kTimestamp2 = 1711461383u;
    constexpr auto kTimestamp3 = 1711461382u;
    constexpr auto kTimestamp4 = 1711461381u;
    mockLedgerObject(*backend_, kAccount, kDocumentID1, kTx1, 1e3, 2, kTimestamp1);  // 10
    mockLedgerObject(*backend_, kAccount, kDocumentID2, kTx1, 2e3, 2, kTimestamp2);  // 20
    mockLedgerObject(*backend_, kAccount, kDocumentID4, kTx1, 4e2, 1, kTimestamp3);  // 40
    mockLedgerObject(*backend_, kAccount, kDocumentID3, kTx1, 3e3, 1, kTimestamp4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "time_threshold": {},
                "oracles": [
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
            }})JSON",
            kTimestamp1 - kTimestamp2,
            kAccount,
            kDocumentID1,
            kAccount,
            kDocumentID2,
            kAccount,
            kDocumentID3,
            kAccount,
            kDocumentID4
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "15",
                    "size": 2,
                    "standard_deviation": "7.071067811865475245"
                }},
                "median": "15",
                "time": {},
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kTimestamp1,
            kRangeMax,
            kLedgerHash
        )
    );
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, TimeThresholdTooLong)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentID1 = 1;
    constexpr auto kDocumentID2 = 2;
    constexpr auto kDocumentID3 = 3;
    constexpr auto kDocumentID4 = 4;
    constexpr auto kTimestamp1 = 1711461384u;
    constexpr auto kTimestamp2 = 1711461383u;
    constexpr auto kTimestamp3 = 1711461382u;
    constexpr auto kTimestamp4 = 1711461381u;
    mockLedgerObject(*backend_, kAccount, kDocumentID1, kTx1, 1e3, 2, kTimestamp1);  // 10
    mockLedgerObject(*backend_, kAccount, kDocumentID2, kTx1, 2e3, 2, kTimestamp2);  // 20
    mockLedgerObject(*backend_, kAccount, kDocumentID4, kTx1, 4e2, 1, kTimestamp3);  // 40
    mockLedgerObject(*backend_, kAccount, kDocumentID3, kTx1, 3e3, 1, kTimestamp4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "time_threshold": {},
                "oracles": [
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
            }})JSON",
            kTimestamp1 + 1,
            kAccount,
            kDocumentID1,
            kAccount,
            kDocumentID2,
            kAccount,
            kDocumentID3,
            kAccount,
            kDocumentID4
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "92.5",
                    "size": 4,
                    "standard_deviation": "138.8944443333377776"
                }},
                "median": "30",
                "time": 1711461384,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kRangeMax,
            kLedgerHash
        )
    );
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

TEST_F(RPCGetAggregatePriceHandlerTest, TimeThresholdIncludeOldest)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentID1 = 1;
    constexpr auto kDocumentID2 = 2;
    constexpr auto kDocumentID3 = 3;
    constexpr auto kDocumentID4 = 4;
    constexpr auto kTimestamp1 = 1711461384u;
    constexpr auto kTimestamp2 = 1711461383u;
    constexpr auto kTimestamp3 = 1711461382u;
    constexpr auto kTimestamp4 = 1711461381u;
    mockLedgerObject(*backend_, kAccount, kDocumentID1, kTx1, 1e3, 2, kTimestamp1);  // 10
    mockLedgerObject(*backend_, kAccount, kDocumentID2, kTx1, 2e3, 2, kTimestamp2);  // 20
    mockLedgerObject(*backend_, kAccount, kDocumentID4, kTx1, 4e2, 1, kTimestamp3);  // 40
    mockLedgerObject(*backend_, kAccount, kDocumentID3, kTx1, 3e3, 1, kTimestamp4);  // 300

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "USD",
                "quote_asset": "XRP",
                "time_threshold": {},
                "oracles": [
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
            }})JSON",
            kTimestamp4 - kTimestamp1,
            kAccount,
            kDocumentID1,
            kAccount,
            kDocumentID2,
            kAccount,
            kDocumentID3,
            kAccount,
            kDocumentID4
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "92.5",
                    "size": 4,
                    "standard_deviation": "138.8944443333377776"
                }},
                "median": "30",
                "time": 1711461384,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kRangeMax,
            kLedgerHash
        )
    );
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}

// When the price pair is not available in the current oracle, trace back to previous transactions
TEST_F(RPCGetAggregatePriceHandlerTest, FromTx)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentId = 1;
    auto const oracleIndex =
        xrpl::keylet::oracle(getAccountIdWithString(kAccount), kDocumentId).key;
    mockLedgerObject(*backend_, kAccount, kDocumentId, kTx1, 1e3, 2);  // 10
    // return a tx which contains NewFields
    EXPECT_CALL(*backend_, fetchTransaction(xrpl::uint256(kTx1), _))
        .WillOnce(Return(createOracleSetTxWithMetadata(
            kAccount,
            kRangeMax,
            123,
            1,
            4321u,
            createPriceDataSeries(
                {createOraclePriceData(1e3, xrpl::toCurrency("JPY"), xrpl::toCurrency("XRP"), 2)}
            ),
            xrpl::to_string(oracleIndex),
            false,
            kTx1
        )));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "JPY",
                "quote_asset": "XRP",
                "oracles": [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})JSON",
            kAccount,
            kDocumentId
        )
    );

    auto const expected = boost::json::parse(
        fmt::format(
            R"JSON({{
                "entire_set": {{
                    "mean": "10",
                    "size": 1,
                    "standard_deviation": "0"
                }},
                "median": "10",
                "time": 4321,
                "ledger_index": {},
                "ledger_hash": "{}",
                "validated": true
            }})JSON",
            kRangeMax,
            kLedgerHash
        )
    );
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result.value(), expected);
    });
}
TEST_F(RPCGetAggregatePriceHandlerTest, NotFoundInTxHistory)
{
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _))
        .WillOnce(Return(createLedgerHeader(kLedgerHash, kRangeMax)));

    constexpr auto kDocumentId = 1;
    auto const oracleIndex =
        xrpl::keylet::oracle(getAccountIdWithString(kAccount), kDocumentId).key;
    mockLedgerObject(*backend_, kAccount, kDocumentId, kTx1, 1e3, 2);  // 10

    EXPECT_CALL(*backend_, fetchTransaction(xrpl::uint256(kTx1), _))
        .WillOnce(Return(createOracleSetTxWithMetadata(
            kAccount,
            kRangeMax,
            123,
            1,
            4321u,
            createPriceDataSeries(
                {createOraclePriceData(1e3, xrpl::toCurrency("EUR"), xrpl::toCurrency("XRP"), 2)}
            ),
            xrpl::to_string(oracleIndex),
            false,
            kTx2
        )));

    EXPECT_CALL(*backend_, fetchTransaction(xrpl::uint256(kTx2), _))
        .WillRepeatedly(Return(createOracleSetTxWithMetadata(
            kAccount,
            kRangeMax,
            123,
            1,
            4321u,
            createPriceDataSeries(
                {createOraclePriceData(1e3, xrpl::toCurrency("EUR"), xrpl::toCurrency("XRP"), 2)}
            ),
            xrpl::to_string(oracleIndex),
            false,
            kTx2
        )));

    auto const handler = AnyHandler{GetAggregatePriceHandler{backend_}};
    auto const req = boost::json::parse(
        fmt::format(
            R"JSON({{
                "base_asset": "JPY",
                "quote_asset": "XRP",
                "oracles": [
                    {{
                        "account": "{}",
                        "oracle_document_id": {}
                    }}
                ]
            }})JSON",
            kAccount,
            kDocumentId
        )
    );

    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "The requested object was not found.");
    });
}
