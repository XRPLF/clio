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

#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <rpc/common/AnyHandler.h>
#include <rpc/handlers/VersionHandler.h>

constexpr static auto DEFAULT_API_VERSION = 3u;
constexpr static auto MIN_API_VERSION = 2u;
constexpr static auto MAX_API_VERSION = 10u;

using namespace RPC;
namespace json = boost::json;

class RPCVersionHandlerTest : public HandlerBaseTest
{
};

TEST_F(RPCVersionHandlerTest, Default)
{
    clio::Config cfg{json::parse(fmt::format(
        R"({{
            "min": {},
            "max": {},
            "default": {}
        }})",
        MIN_API_VERSION,
        MAX_API_VERSION,
        DEFAULT_API_VERSION))};

    runSpawn([&](auto yield) {
        auto const handler = AnyHandler{VersionHandler{cfg}};
        auto const output = handler.process(static_cast<json::value>(cfg), Context{yield});
        ASSERT_TRUE(output);

        // check all against all the correct values
        auto const& result = output.value().as_object();
        EXPECT_TRUE(result.contains("version"));
        auto const& info = result.at("version").as_object();
        EXPECT_TRUE(info.contains("first"));
        EXPECT_TRUE(info.contains("last"));
        EXPECT_TRUE(info.contains("good"));
        EXPECT_EQ(info.at("first"), 2u);
        EXPECT_EQ(info.at("last"), 10u);
        EXPECT_EQ(info.at("good"), 3u);
    });
}
