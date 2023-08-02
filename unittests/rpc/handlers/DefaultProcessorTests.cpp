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

#include <rpc/handlers/impl/FakesAndMocks.h>
#include <util/Fixtures.h>

#include <rpc/common/impl/Processors.h>

#include <boost/json/parse.hpp>

using namespace testing;
using namespace std;

using namespace RPC;
using namespace RPC::validation;
using namespace unittests::detail;

namespace json = boost::json;

class RPCDefaultProcessorTest : public HandlerBaseTest
{
};

TEST_F(RPCDefaultProcessorTest, ValidInput)
{
    runSpawn([](auto yield) {
        HandlerMock handler;
        RPC::detail::DefaultProcessor<HandlerMock> processor;

        auto const input = json::parse(R"({ "something": "works" })");
        auto const spec = RpcSpec{{"something", Required{}}};
        auto const data = InOutFake{"works"};
        EXPECT_CALL(handler, spec(_)).WillOnce(ReturnRef(spec));
        EXPECT_CALL(handler, process(Eq(data), _)).WillOnce(Return(data));

        auto const ret = processor(handler, input, Context{yield});
        ASSERT_TRUE(ret);  // no error
    });
}

TEST_F(RPCDefaultProcessorTest, NoInputVaildCall)
{
    runSpawn([](auto yield) {
        HandlerWithoutInputMock handler;
        RPC::detail::DefaultProcessor<HandlerWithoutInputMock> processor;

        auto const data = InOutFake{"works"};
        auto const input = json::parse(R"({})");
        EXPECT_CALL(handler, process(_)).WillOnce(Return(data));

        auto const ret = processor(handler, input, Context{yield});
        ASSERT_TRUE(ret);  // no error
    });
}

TEST_F(RPCDefaultProcessorTest, InvalidInput)
{
    runSpawn([](auto yield) {
        HandlerMock handler;
        RPC::detail::DefaultProcessor<HandlerMock> processor;

        auto const input = json::parse(R"({ "other": "nope" })");
        auto const spec = RpcSpec{{"something", Required{}}};
        EXPECT_CALL(handler, spec(_)).WillOnce(ReturnRef(spec));

        auto const ret = processor(handler, input, Context{yield});
        ASSERT_FALSE(ret);  // returns error
    });
}
