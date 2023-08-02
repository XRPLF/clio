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
#include <rpc/handlers/impl/FakesAndMocks.h>
#include <util/Fixtures.h>
#include <util/MockWsBase.h>

#include <boost/json/parse.hpp>

using namespace std;
using namespace RPC;
using namespace RPC::validation;
using namespace unittests::detail;

namespace json = boost::json;

class RPCTestHandlerTest : public HandlerBaseTest
{
};

// example handler tests
TEST_F(RPCTestHandlerTest, HandlerSuccess)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{HandlerFake{}};
        auto const input = json::parse(R"({ 
            "hello": "world", 
            "limit": 10
        })");

        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);

        auto const val = output.value();
        EXPECT_EQ(val.as_object().at("computed").as_string(), "world_10");
    });
}

TEST_F(RPCTestHandlerTest, NoInputHandlerSuccess)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{NoInputHandlerFake{}};
        auto const output = handler.process(json::parse(R"({})"), Context{yield});
        ASSERT_TRUE(output);

        auto const val = output.value();
        EXPECT_EQ(val.as_object().at("computed").as_string(), "test");
    });
}

TEST_F(RPCTestHandlerTest, HandlerErrorHandling)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{HandlerFake{}};
        auto const input = json::parse(R"({ 
            "hello": "not world", 
            "limit": 10
        })");

        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid parameters.");
        EXPECT_EQ(err.at("error_code").as_uint64(), 31);
    });
}

TEST_F(RPCTestHandlerTest, HandlerInnerErrorHandling)
{
    runSpawn([](auto yield) {
        auto const handler = AnyHandler{FailingHandlerFake{}};
        auto const input = json::parse(R"({ 
            "hello": "world", 
            "limit": 10
        })");

        // validation succeeds but handler itself returns error
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "Very custom error");
    });
}
