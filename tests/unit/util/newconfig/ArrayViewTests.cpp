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

#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/FakeConfigData.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/ValueView.hpp"

#include <gtest/gtest.h>

#include <cstddef>

using namespace util::config;

struct ArrayViewTest : testing::Test {
    ClioConfigDefinition const configData = generateConfig();
};

TEST_F(ArrayViewTest, ArrayValueTest)
{
    ArrayView const arrVals = configData.getArray("array.[].sub");
    auto valIt = arrVals.begin<ValueView>();
    auto const precision = 1e-9;
    EXPECT_NEAR((*valIt++).asDouble(), 111.11, precision);
    EXPECT_NEAR((*valIt++).asDouble(), 4321.55, precision);
    EXPECT_EQ(valIt, arrVals.end<ValueView>());

    EXPECT_NEAR(111.11, arrVals.valueAt(0).asDouble(), precision);
    EXPECT_NEAR(4321.55, arrVals.valueAt(1).asDouble(), precision);

    ArrayView const arrVals2 = configData.getArray("array.[].sub2");
    auto val2It = arrVals2.begin<ValueView>();
    EXPECT_EQ((*val2It++).asString(), "subCategory");
    EXPECT_EQ((*val2It++).asString(), "temporary");
    EXPECT_EQ(val2It, arrVals2.end<ValueView>());

    ValueView const tempVal = arrVals2.valueAt(0);
    EXPECT_EQ(tempVal.type(), ConfigType::String);
    EXPECT_EQ("subCategory", tempVal.asString());
}

TEST_F(ArrayViewTest, ArrayWithObjTest)
{
    ArrayView const arrVals = configData.getArray("array.[]");
    ArrayView const arrValAlt = configData.getArray("array");
    auto const precision = 1e-9;

    auto const obj1 = arrVals.objectAt(0);
    auto const obj2 = arrValAlt.objectAt(0);
    EXPECT_NEAR(obj1.getValue("sub").asDouble(), obj2.getValue("sub").asDouble(), precision);
    EXPECT_NEAR(obj1.getValue("sub").asDouble(), 111.11, precision);
}

TEST_F(ArrayViewTest, IterateArray)
{
    auto arr = configData.getArray("dosguard.whitelist");
    EXPECT_EQ(2, arr.size());
    EXPECT_EQ(arr.valueAt(0).asString(), "125.5.5.2");
    EXPECT_EQ(arr.valueAt(1).asString(), "204.2.2.2");

    auto it = arr.begin<ValueView>();
    EXPECT_EQ((*it++).asString(), "125.5.5.2");
    EXPECT_EQ((*it++).asString(), "204.2.2.2");
    EXPECT_EQ((it), arr.end<ValueView>());
}

TEST_F(ArrayViewTest, DifferentArrayIterators)
{
    auto const subArray = configData.getArray("array.[].sub");
    auto const dosguardArray = configData.getArray("dosguard.whitelist.[]");
    ASSERT_EQ(subArray.size(), dosguardArray.size());

    auto itArray = subArray.begin<ValueView>();
    auto itDosguard = dosguardArray.begin<ValueView>();

    for (std::size_t i = 0; i < subArray.size(); i++)
        EXPECT_NE(itArray++, itDosguard++);
}

TEST_F(ArrayViewTest, IterateObject)
{
    auto arr = configData.getArray("array");
    EXPECT_EQ(2, arr.size());

    auto it = arr.begin<ObjectView>();
    EXPECT_EQ(111.11, (*it).getValue("sub").asDouble());
    EXPECT_EQ("subCategory", (*it++).getValue("sub2").asString());

    EXPECT_EQ(4321.55, (*it).getValue("sub").asDouble());
    EXPECT_EQ("temporary", (*it++).getValue("sub2").asString());

    EXPECT_EQ(it, arr.end<ObjectView>());
}

struct ArrayViewDeathTest : ArrayViewTest {};

TEST_F(ArrayViewDeathTest, IncorrectAccess)
{
    ArrayView const arr = configData.getArray("higher");

    // dies because higher only has 1 object
    EXPECT_DEATH({ [[maybe_unused]] auto _ = arr.objectAt(1); }, ".*");

    ArrayView const arrVals2 = configData.getArray("array.[].sub2");
    ValueView const tempVal = arrVals2.valueAt(0);

    // dies because array.[].sub2 only has 2 config values
    EXPECT_DEATH([[maybe_unused]] auto _ = arrVals2.valueAt(2), ".*");

    // dies as value is not of type int
    EXPECT_DEATH({ [[maybe_unused]] auto _ = tempVal.asIntType<int>(); }, ".*");
}

TEST_F(ArrayViewDeathTest, IncorrectIterateAccess)
{
    ArrayView const arr = configData.getArray("higher");
    EXPECT_DEATH({ [[maybe_unused]] auto _ = arr.begin<ValueView>(); }, ".*");

    ArrayView const dosguardWhitelist = configData.getArray("dosguard.whitelist");
    EXPECT_DEATH({ [[maybe_unused]] auto _ = dosguardWhitelist.begin<ObjectView>(); }, ".*");
}
