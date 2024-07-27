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
#include "util/newconfig/ValueView.hpp"

#include <gtest/gtest.h>

using namespace util::config;

TEST(ArrayTest, testConfigArray)
{
    auto arr = Array{
        ConfigValue{ConfigType::Boolean}.defaultValue(false),
        ConfigValue{ConfigType::Integer}.defaultValue(1234),
        ConfigValue{ConfigType::Double}.defaultValue(22.22),
    };
    auto cv = arr.at(0);
    ValueView vv{cv};
    EXPECT_EQ(vv.asBool(), false);

    auto cv2 = arr.at(1);
    ValueView vv2{cv2};
    EXPECT_EQ(vv2.asIntType<int>(), 1234);

    EXPECT_EQ(arr.size(), 3);
    arr.emplace_back(ConfigValue{ConfigType::String}.defaultValue("false"));

    EXPECT_EQ(arr.size(), 4);
    auto cv4 = arr.at(3);
    ValueView vv4{cv4};
    EXPECT_EQ(vv4.asString(), "false");
}
