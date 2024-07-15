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
    ClioConfigDefinition configData = generateConfig();
};

TEST_F(ArrayViewTest, ArrayValueTest)
{
    ArrayView const arrVals = configData.getArray("array.[].sub");
    auto valIt = arrVals.beginValues();
    ASSERT_EQ((*valIt++).asDouble(), 111.11);
    ASSERT_EQ((*valIt++).asDouble(), 4321.55);
    ASSERT_EQ(valIt, arrVals.endValues());

    ASSERT_EQ(111.11, arrVals.valueAt(0).asDouble());
    ASSERT_EQ(4321.55, arrVals.valueAt(1).asDouble());

    ArrayView arrVals2 = configData.getArray("array.[].sub2");
    auto val2It = arrVals2.beginValues();
    ASSERT_EQ((*val2It++).asString(), "subCategory");
    ASSERT_EQ((*val2It++).asString(), "temporary");
    ASSERT_EQ(val2It, arrVals2.endValues());

    ValueView const tempVal = arrVals2.valueAt(0);
    ASSERT_EQ(tempVal.type(), ConfigType::String);
    ASSERT_EQ("subCategory", tempVal.asString());

    ASSERT_THROW(tempVal.asInt(), std::bad_variant_access);
}

struct ArrayViewDeathTest : ArrayViewTest {};

TEST_F(ArrayViewDeathTest, IncorrectAccess)
{
    ArrayView arr = configData.getArray("higher");
    ASSERT_DEATH(arr.objectAt(1), "");
    ASSERT_DEATH(arr.beginValues(), "");
}
