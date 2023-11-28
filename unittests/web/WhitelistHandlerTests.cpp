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
#include "util/Fixtures.h"
#include "util/config/Config.h"
#include "web/WhitelistHandler.h"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

using namespace util;
using namespace web;

constexpr static auto JSONDataIPV4 = R"JSON(
    {
        "dos_guard": {
            "whitelist": [
                "127.0.0.1",
                "192.168.0.1/22", 
                "10.0.0.1"
            ]
        }
    }
)JSON";

constexpr static auto JSONDataIPV6 = R"JSON(
    {
        "dos_guard": {
            "whitelist": [
                "2002:1dd8:85a7:0000:0000:8a6e:0000:1111",
                "2001:0db8:85a3:0000:0000:8a2e:0000:0000/22"
            ]
        }
    }
)JSON";

class WhitelistHandlerTest : public NoLoggerFixture {};

TEST_F(WhitelistHandlerTest, TestWhiteListIPV4)
{
    Config const cfg{boost::json::parse(JSONDataIPV4)};
    WhitelistHandler const whitelistHandler{cfg};

    EXPECT_TRUE(whitelistHandler.isWhiteListed("192.168.1.10"));
    EXPECT_FALSE(whitelistHandler.isWhiteListed("193.168.0.123"));
    EXPECT_TRUE(whitelistHandler.isWhiteListed("10.0.0.1"));
    EXPECT_FALSE(whitelistHandler.isWhiteListed("10.0.0.2"));
}

TEST_F(WhitelistHandlerTest, TestWhiteListIPV6)
{
    Config const cfg{boost::json::parse(JSONDataIPV6)};
    WhitelistHandler const whitelistHandler{cfg};

    EXPECT_TRUE(whitelistHandler.isWhiteListed("2002:1dd8:85a7:0000:0000:8a6e:0000:1111"));
    EXPECT_FALSE(whitelistHandler.isWhiteListed("2002:1dd8:85a7:1101:0000:8a6e:0000:1111"));
    EXPECT_TRUE(whitelistHandler.isWhiteListed("2001:0db8:85a3:0000:0000:8a2e:0000:0000"));
    EXPECT_TRUE(whitelistHandler.isWhiteListed("2001:0db8:85a3:0000:1111:8a2e:0370:7334"));
}
