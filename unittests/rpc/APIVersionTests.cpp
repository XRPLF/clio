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

#include <rpc/common/impl/APIVersionParser.h>

#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>

constexpr static auto DEFAULT_API_VERSION = 5u;
constexpr static auto MIN_API_VERSION = 2u;
constexpr static auto MAX_API_VERSION = 10u;

using namespace RPC::detail;
namespace json = boost::json;

class RPCAPIVersionTest : public NoLoggerFixture
{
protected:
    ProductionAPIVersionParser parser{DEFAULT_API_VERSION, MIN_API_VERSION, MAX_API_VERSION};
};

TEST_F(RPCAPIVersionTest, ReturnsDefaultVersionIfNotSpecified)
{
    auto ver = parser.parse(json::parse("{}").as_object());
    EXPECT_TRUE(ver);
    EXPECT_EQ(ver.value(), DEFAULT_API_VERSION);
}

TEST_F(RPCAPIVersionTest, ReturnsErrorIfVersionHigherThanMaxSupported)
{
    auto ver = parser.parse(json::parse(R"({"api_version": 11})").as_object());
    EXPECT_FALSE(ver);
}

TEST_F(RPCAPIVersionTest, ReturnsErrorIfVersionLowerThanMinSupported)
{
    auto ver = parser.parse(json::parse(R"({"api_version": 1})").as_object());
    EXPECT_FALSE(ver);
}

TEST_F(RPCAPIVersionTest, ReturnsErrorOnWrongType)
{
    {
        auto ver = parser.parse(json::parse(R"({"api_version": null})").as_object());
        EXPECT_FALSE(ver);
    }
    {
        auto ver = parser.parse(json::parse(R"({"api_version": "5"})").as_object());
        EXPECT_FALSE(ver);
    }
    {
        auto ver = parser.parse(json::parse(R"({"api_version": "wrong"})").as_object());
        EXPECT_FALSE(ver);
    }
}

TEST_F(RPCAPIVersionTest, ReturnsParsedVersionIfAllPreconditionsAreMet)
{
    {
        auto ver = parser.parse(json::parse(R"({"api_version": 2})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 2u);
    }
    {
        auto ver = parser.parse(json::parse(R"({"api_version": 10})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 10u);
    }
    {
        auto ver = parser.parse(json::parse(R"({"api_version": 5})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 5u);
    }
}

TEST_F(RPCAPIVersionTest, GetsValuesFromConfigCorrectly)
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

    ProductionAPIVersionParser configuredParser{cfg};

    {
        auto ver = configuredParser.parse(json::parse(R"({"api_version": 2})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 2u);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"({"api_version": 10})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 10u);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"({"api_version": 5})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 5u);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"({})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), DEFAULT_API_VERSION);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"({"api_version": 11})").as_object());
        EXPECT_FALSE(ver);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"({"api_version": 1})").as_object());
        EXPECT_FALSE(ver);
    }
}
