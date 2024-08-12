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

#include "util/newconfig/Array.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "util/newconfig/ValueView.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace util::config;

TEST(ArrayTest, testConfigArray)
{
    auto arr = Array{ConfigValue{ConfigType::Double}};
    arr.emplaceBack(ConfigValue{ConfigType::Double}.defaultValue(111.11));
    EXPECT_EQ(arr.size(), 1);

    arr.emplaceBack(ConfigValue{ConfigType::Double}.defaultValue(222.22));
    arr.emplaceBack(ConfigValue{ConfigType::Double}.defaultValue(333.33));
    EXPECT_EQ(arr.size(), 3);

    auto const cv = arr.at(0);
    ValueView vv{cv};
    EXPECT_EQ(vv.asDouble(), 111.11);

    auto const cv2 = arr.at(1);
    ValueView vv2{cv2};
    EXPECT_EQ(vv2.asDouble(), 222.22);

    EXPECT_EQ(arr.size(), 3);
    arr.emplaceBack(ConfigValue{ConfigType::Double}.defaultValue(444.44));

    EXPECT_EQ(arr.size(), 4);
    auto const cv4 = arr.at(3);
    ValueView vv4{cv4};
    EXPECT_EQ(vv4.asDouble(), 444.44);
}

TEST(ArrayTest, iterateArray)
{
    auto arr = Array{ConfigValue{ConfigType::Integer}};
    std::vector<int64_t> const expected{543, 123, 909};

    for (auto const num : expected)
        arr.emplaceBack(ConfigValue{ConfigType::Integer}.defaultValue(num));

    std::vector<int64_t> actual;
    for (auto it = arr.begin(); it != arr.end(); ++it)
        actual.emplace_back(std::get<int64_t>(it->getValue()));

    EXPECT_TRUE(std::ranges::equal(expected, actual));
}
