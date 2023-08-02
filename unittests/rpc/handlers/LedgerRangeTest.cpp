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
#include <rpc/handlers/LedgerRange.h>
#include <util/Fixtures.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto RANGEMIN = 10;
constexpr static auto RANGEMAX = 30;

class RPCLedgerRangeTest : public HandlerBaseTest
{
};

TEST_F(RPCLedgerRangeTest, LedgerRangeMinMaxSame)
{
    runSpawn([this](auto yield) {
        mockBackendPtr->updateRange(RANGEMIN);
        auto const handler = AnyHandler{LedgerRangeHandler{mockBackendPtr}};
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        auto const json = output.value();
        EXPECT_EQ(json.at("ledger_index_min").as_uint64(), RANGEMIN);
        EXPECT_EQ(json.at("ledger_index_max").as_uint64(), RANGEMIN);
    });
}

TEST_F(RPCLedgerRangeTest, LedgerRangeFullySet)
{
    runSpawn([this](auto yield) {
        mockBackendPtr->updateRange(RANGEMIN);
        mockBackendPtr->updateRange(RANGEMAX);
        auto const handler = AnyHandler{LedgerRangeHandler{mockBackendPtr}};
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        auto const json = output.value();
        EXPECT_EQ(json.at("ledger_index_min").as_uint64(), RANGEMIN);
        EXPECT_EQ(json.at("ledger_index_max").as_uint64(), RANGEMAX);
    });
}
