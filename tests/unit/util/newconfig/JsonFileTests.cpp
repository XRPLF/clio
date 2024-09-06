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

#include "util/TmpFile.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/FakeConfigData.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

struct ParseJson : testing::Test {
    ParseJson() : jsonFileObj{TmpFile(JSONData).path}
    {
    }

    ConfigFileJson const jsonFileObj;
};

TEST_F(ParseJson, validateValues)
{
    EXPECT_TRUE(jsonFileObj.containsKey("header.text1"));
    EXPECT_EQ(std::get<std::string>(jsonFileObj.getValue("header.text1")), "value");

    EXPECT_TRUE(jsonFileObj.containsKey("header.sub.sub2Value"));
    EXPECT_EQ(std::get<std::string>(jsonFileObj.getValue("header.sub.sub2Value")), "TSM");

    EXPECT_TRUE(jsonFileObj.containsKey("dosguard.port"));
    EXPECT_EQ(std::get<int64_t>(jsonFileObj.getValue("dosguard.port")), 44444);

    EXPECT_FALSE(jsonFileObj.containsKey("idk"));
    EXPECT_FALSE(jsonFileObj.containsKey("optional.withNoDefault"));
}

TEST_F(ParseJson, validateArrayValue)
{
    // validate array.[].sub matches expected values
    EXPECT_TRUE(jsonFileObj.containsKey("array.[].sub"));
    auto const arrSub = jsonFileObj.getArray("array.[].sub");
    EXPECT_EQ(arrSub.size(), 3);

    std::vector<double> expectedArrSubVal{111.11, 4321.55, 5555.44};
    std::vector<double> actualArrSubVal{};

    for (auto it = arrSub.begin(); it != arrSub.end(); ++it) {
        ASSERT_TRUE(std::holds_alternative<double>(*it));
        actualArrSubVal.emplace_back(std::get<double>(*it));
    }
    EXPECT_TRUE(std::ranges::equal(expectedArrSubVal, actualArrSubVal));

    // validate array.[].sub2 matches expected values
    EXPECT_TRUE(jsonFileObj.containsKey("array.[].sub2"));
    auto const arrSub2 = jsonFileObj.getArray("array.[].sub2");
    EXPECT_EQ(arrSub2.size(), 3);
    std::vector<std::string> expectedArrSub2Val{"subCategory", "temporary", "london"};
    std::vector<std::string> actualArrSub2Val{};

    for (auto it = arrSub2.begin(); it != arrSub2.end(); ++it) {
        ASSERT_TRUE(std::holds_alternative<std::string>(*it));
        actualArrSub2Val.emplace_back(std::get<std::string>(*it));
    }
    EXPECT_TRUE(std::ranges::equal(expectedArrSub2Val, actualArrSub2Val));

    EXPECT_TRUE(jsonFileObj.containsKey("dosguard.whitelist.[]"));
    auto const whitelistArr = jsonFileObj.getArray("dosguard.whitelist.[]");
    EXPECT_EQ(whitelistArr.size(), 2);
    EXPECT_EQ("125.5.5.1", std::get<std::string>(whitelistArr.at(0)));
    EXPECT_EQ("204.2.2.1", std::get<std::string>(whitelistArr.at(1)));
}

struct JsonValueDeathTest : ParseJson {};

TEST_F(JsonValueDeathTest, invalidGetValues)
{
    EXPECT_DEATH([[maybe_unused]] auto a = jsonFileObj.getValue("doesn't exist"), ".*");
    EXPECT_DEATH([[maybe_unused]] auto a = jsonFileObj.getArray("header.text1"), ".*");
}
