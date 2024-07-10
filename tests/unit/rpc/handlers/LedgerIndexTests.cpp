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
#include "rpc/handlers/LedgerIndex.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdlib.h>

#include <cstdint>
#include <string>
#include <vector>

constexpr static auto RANGEMIN = 10;
constexpr static auto RANGEMAX = 30;
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

using namespace rpc;
namespace json = boost::json;
using namespace testing;

class RPCLedgerIndexTest : public HandlerBaseTestStrict {};

TEST_F(RPCLedgerIndexTest, DateStrNotValid)
{
    auto const handler = AnyHandler{LedgerIndexHandler{backend}};
    auto const req = json::parse(R"({"date": "not_a_number"})");
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid parameters.");
    });
}

TEST_F(RPCLedgerIndexTest, NoDateGiven)
{
    backend->setRange(RANGEMIN, RANGEMAX);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, RANGEMAX, 5);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillOnce(Return(ledgerHeader));

    auto const handler = AnyHandler{LedgerIndexHandler{backend}};
    auto const req = json::parse(R"({})");
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger_index").as_uint64(), RANGEMAX);
        EXPECT_EQ(output.result->at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_TRUE(output.result->as_object().contains("closed"));
    });
}

TEST_F(RPCLedgerIndexTest, EarlierThanMinLedger)
{
    backend->setRange(RANGEMIN, RANGEMAX);
    auto const handler = AnyHandler{LedgerIndexHandler{backend}};
    auto const req = json::parse(R"({"date": "2024-06-25T12:23:05Z"})");
    auto const ledgerHeader =
        CreateLedgerHeaderWithUnixTime(LEDGERHASH, RANGEMIN, 1719318190);  //"2024-06-25T12:23:10Z"
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMIN, _)).WillOnce(Return(ledgerHeader));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
    });
}

TEST_F(RPCLedgerIndexTest, ChangeTimeZone)
{
    setenv("TZ", "EST+5", 1);
    backend->setRange(RANGEMIN, RANGEMAX);
    auto const handler = AnyHandler{LedgerIndexHandler{backend}};
    auto const req = json::parse(R"({"date": "2024-06-25T12:23:05Z"})");
    auto const ledgerHeader =
        CreateLedgerHeaderWithUnixTime(LEDGERHASH, RANGEMIN, 1719318190);  //"2024-06-25T12:23:10Z"
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMIN, _)).WillOnce(Return(ledgerHeader));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
    });
    unsetenv("TZ");
}

struct LedgerIndexTestsCaseBundle {
    std::string testName;
    std::string json;
    std::uint32_t expectedLedgerIndex;
    std::string closeTimeIso;
};

class LedgerIndexTests : public RPCLedgerIndexTest, public WithParamInterface<LedgerIndexTestsCaseBundle> {
public:
    static auto
    generateTestValuesForParametersTest()
    {
        // start from 2024-06-25T12:23:10Z to 2024-06-25T12:23:50Z with step 2
        return std::vector<LedgerIndexTestsCaseBundle>{
            {"LaterThanMaxLedger", R"({"date": "2024-06-25T12:23:55Z"})", RANGEMAX, "2024-06-25T12:23:50Z"},
            {"GreaterThanMinLedger", R"({"date": "2024-06-25T12:23:11Z"})", RANGEMIN, "2024-06-25T12:23:10Z"},
            {"IsMinLedger", R"({"date": "2024-06-25T12:23:10Z"})", RANGEMIN, "2024-06-25T12:23:10Z"},
            {"IsMaxLedger", R"({"date": "2024-06-25T12:23:50Z"})", RANGEMAX, "2024-06-25T12:23:50Z"},
            {"IsMidLedger", R"({"date": "2024-06-25T12:23:30Z"})", 20, "2024-06-25T12:23:30Z"},
            {"BetweenLedgers", R"({"date": "2024-06-25T12:23:29Z"})", 19, "2024-06-25T12:23:28Z"}
        };
    }
};

INSTANTIATE_TEST_CASE_P(
    RPCLedgerIndexTestsGroup,
    LedgerIndexTests,
    ValuesIn(LedgerIndexTests::generateTestValuesForParametersTest()),
    tests::util::NameGenerator
);

TEST_P(LedgerIndexTests, SearchFromLedgerRange)
{
    auto const testBundle = GetParam();
    auto const jv = json::parse(testBundle.json).as_object();
    backend->setRange(RANGEMIN, RANGEMAX);

    // start from 1719318190 , which is the unix time for 2024-06-25T12:23:10Z to 2024-06-25T12:23:50Z with
    // step 2
    for (uint32_t i = RANGEMIN; i <= RANGEMAX; i++) {
        auto const ledgerHeader = CreateLedgerHeaderWithUnixTime(LEDGERHASH, i, 1719318190 + 2 * (i - RANGEMIN));
        auto const exactNumberOfCalls = i == RANGEMIN ? Exactly(3) : Exactly(2);
        EXPECT_CALL(*backend, fetchLedgerBySequence(i, _))
            .Times(i == testBundle.expectedLedgerIndex ? exactNumberOfCalls : AtMost(1))
            .WillRepeatedly(Return(ledgerHeader));
    }

    auto const handler = AnyHandler{LedgerIndexHandler{backend}};
    auto const req = json::parse(testBundle.json);
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger_index").as_uint64(), testBundle.expectedLedgerIndex);
        EXPECT_EQ(output.result->at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_EQ(output.result->at("closed").as_string(), testBundle.closeTimeIso);
    });
}
