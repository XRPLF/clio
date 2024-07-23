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
#include "util/newconfig/ObjectView.hpp"
#include "util/newconfig/ValueView.hpp"

#include <../common/newconfig/FakeConfigData.hpp>
#include <gtest/gtest.h>

#include <variant>

using namespace util::config;

struct ArrayViewTest : testing::Test {
    ClioConfigDefinition const configData = generateConfig();
};

TEST_F(ArrayViewTest, ArrayValueTest)
{
    ArrayView const arrVals = configData.getArray("array.[].sub");
    auto valIt = arrVals.beginValues();
    EXPECT_NEAR((*valIt++).asDouble(), 111.11, 1e-9);
    EXPECT_NEAR((*valIt++).asDouble(), 4321.55, 1e-9);
    EXPECT_EQ(valIt, arrVals.endValues());

    EXPECT_NEAR(111.11, arrVals.valueAt(0).asDouble(), 1e-9);
    EXPECT_NEAR(4321.55, arrVals.valueAt(1).asDouble(), 1e-9);

    ArrayView const arrVals2 = configData.getArray("array.[].sub2");
    auto val2It = arrVals2.beginValues();
    EXPECT_EQ((*val2It++).asString(), "subCategory");
    EXPECT_EQ((*val2It++).asString(), "temporary");
    EXPECT_EQ(val2It, arrVals2.endValues());

    ValueView const tempVal = arrVals2.valueAt(0);
    EXPECT_EQ(tempVal.type(), ConfigType::String);
    EXPECT_EQ("subCategory", tempVal.asString());

    EXPECT_THROW({ [[maybe_unused]] auto _ = tempVal.asIntType<int>(); }, std::bad_variant_access);
}

TEST_F(ArrayViewTest, ArrayWithObjTest)
{
    ArrayView const arrVals = configData.getArray("array.[]");
    ArrayView const arrValAlt = configData.getArray("array");

    auto const obj1 = arrVals.objectAt(0);
    auto const obj2 = arrValAlt.objectAt(0);
    EXPECT_NEAR(obj1.getValue("sub").asDouble(), obj2.getValue("sub").asDouble(), 1e-9);
    EXPECT_NEAR(obj1.getValue("sub").asDouble(), 111.11, 1e-9);
}

TEST_F(ArrayViewTest, ObjectWithArray)
{
    auto arr = configData.getArray("dosguard.whitelist");
    EXPECT_EQ(2, arr.size());
    EXPECT_EQ(arr.valueAt(0).asString(), "125.5.5.2");
    EXPECT_EQ(arr.valueAt(1).asString(), "204.2.2.2");

    auto const obj2 = configData.getObject("higher.[].low");
    auto const arr2 = obj2.getArray("section");

    auto it = arr2.beginValues();
    EXPECT_EQ((*it++).asString(), "true");
    EXPECT_EQ(it, arr.endValues());
}

struct ArrayViewDeathTest : ArrayViewTest {};

TEST_F(ArrayViewDeathTest, IncorrectAccess)
{
    ArrayView const arr = configData.getArray("higher");
    EXPECT_DEATH({ [[maybe_unused]] auto _ = arr.objectAt(1); }, "");
    EXPECT_DEATH({ [[maybe_unused]] auto _ = arr.beginValues(); }, "");
}
