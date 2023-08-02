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

#include <rpc/RPCHelpers.h>
#include <rpc/common/AnyHandler.h>
#include <rpc/handlers/Random.h>
#include <util/Fixtures.h>

using namespace RPC;

class RPCRandomHandlerTest : public HandlerBaseTest
{
};

TEST_F(RPCRandomHandlerTest, Default)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{RandomHandler{}};
        auto const output = handler.process(boost::json::parse(R"({})"), Context{yield});
        ASSERT_TRUE(output);
        EXPECT_TRUE(output->as_object().contains(JS(random)));
        EXPECT_EQ(output->as_object().at(JS(random)).as_string().size(), 64u);
    });
}
