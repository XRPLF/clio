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
#include "util/Fixtures.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

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
        EXPECT_TRUE(output.result->as_object().contains("close_time_iso"));
    });
}

TEST_F(RPCLedgerIndexTest, ValidDate)
{
    backend->setRange(RANGEMIN, RANGEMAX);
    auto const ledgerHeader = CreateLedgerHeader(LEDGERHASH, RANGEMAX, 5);
    EXPECT_CALL(*backend, fetchLedgerBySequence(RANGEMAX, _)).WillOnce(Return(ledgerHeader));

    auto const handler = AnyHandler{LedgerIndexHandler{backend}};
    auto const req = json::parse(R"({"date": "2024-01-01T00:00:00Z"})");
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger_index").as_uint64(), RANGEMAX);
        EXPECT_EQ(output.result->at("ledger_hash").as_string(), LEDGERHASH);
        EXPECT_TRUE(output.result->as_object().contains("close_time_iso"));
    });
}
