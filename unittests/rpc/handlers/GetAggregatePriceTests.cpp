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

#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/GetAggregatePrice.hpp"
#include "rpc/handlers/LedgerData.hpp"
#include "util/Fixtures.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/AccountID.h>

#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

constexpr static auto RANGEMIN = 10;
constexpr static auto RANGEMAX = 30;

class RPCGetAggregatePriceHandlerTest : public HandlerBaseTest {};

struct GetAggregatePriceParamTestCaseBundle {
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct GetAggregatePriceParameterTest : public RPCGetAggregatePriceHandlerTest,
                                        public WithParamInterface<GetAggregatePriceParamTestCaseBundle> {
    struct NameGenerator {
        template <class ParamType>
        std::string
        operator()(testing::TestParamInfo<ParamType> const& info) const
        {
            auto bundle = static_cast<GetAggregatePriceParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

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
            R"({"base_asset": "XRP", "quote_asset": "USD"})",
            "invalidParams",
            "Required field 'oracles' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            "no_base_asset",
            R"({"quote_asset": "USD", "oracles": {"account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD", "oracle_document_id": 2}})",
            "invalidParams",
            "Required field 'base_asset' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            "no_quote_asset",
            R"({"base_asset": "USD", "oracles": {"account": "rGh1VZCRBJY6rJiaFpD4LZtyHiuCkC8aeD", "oracle_document_id": 2}})",
            "invalidParams",
            "Required field 'quote_asset' missing"
        },
        GetAggregatePriceParamTestCaseBundle{
            "oraclesIsEmpty",
            R"({"base_asset": "USD", "quote_asset": "XRP", "oracles": {}})",
            "oracleMalformed",
            "Oracle request is malformed."
        },
        GetAggregatePriceParamTestCaseBundle{
            "oraclesNotArray",
            R"({"base_asset": "USD", "quote_asset": "XRP", "oracles": 1})",
            "oracleMalformed",
            "Oracle request is malformed."
        },
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCGetAggregatePriceGroup1,
    GetAggregatePriceParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    GetAggregatePriceParameterTest::NameGenerator{}
);

TEST_P(GetAggregatePriceParameterTest, InvalidParams)
{
    backend->setRange(RANGEMIN, RANGEMAX);
    auto const testBundle = GetParam();
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{GetAggregatePriceHandler{backend}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
        std::cout << err << std::endl;
    });
}
