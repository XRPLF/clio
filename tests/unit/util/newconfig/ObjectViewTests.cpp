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
#include "util/newconfig/ObjectView.hpp"

#include <gtest/gtest.h>
#include <newconfig/FakeConfigData.hpp>

using namespace util::config;

struct ObjectViewTest : testing::Test {
    ClioConfigDefinition const configData = generateConfig();
};

TEST_F(ObjectViewTest, ObjectValueTest)
{
    auto const headerObj = configData.getObject("header");
    EXPECT_FALSE(headerObj.containsKey("header"));
    EXPECT_TRUE(headerObj.containsKey("text1"));
    EXPECT_TRUE(headerObj.containsKey("port"));
    EXPECT_TRUE(headerObj.containsKey("admin"));

    EXPECT_EQ("value", headerObj.getValue("text1").asString());
    EXPECT_EQ(123, headerObj.getValue("port").asIntType<int>());
    EXPECT_EQ(true, headerObj.getValue("admin").asBool());
}

TEST_F(ObjectViewTest, ObjectInArray)
{
    ArrayView const arr = configData.getArray("array");
    EXPECT_EQ(arr.size(), 2);
    ObjectView const firstObj = arr.objectAt(0);
    ObjectView const secondObj = arr.objectAt(1);
    EXPECT_TRUE(firstObj.containsKey("sub"));
    EXPECT_TRUE(firstObj.containsKey("sub2"));

    // object's key is only "sub" and "sub2"
    EXPECT_FALSE(firstObj.containsKey("array.[].sub"));

    EXPECT_EQ(firstObj.getValue("sub").asDouble(), 111.11);
    EXPECT_EQ(firstObj.getValue("sub2").asString(), "subCategory");

    EXPECT_EQ(secondObj.getValue("sub").asDouble(), 4321.55);
    EXPECT_EQ(secondObj.getValue("sub2").asString(), "temporary");
}

TEST_F(ObjectViewTest, ObjectInArrayMoreComplex)
{
    ArrayView const arr = configData.getArray("higher");
    ASSERT_EQ(1, arr.size());

    ObjectView const firstObj = arr.objectAt(0);

    // this returns the 1st object inside "low"
    ObjectView const sameObjFromConfigData = configData.getObject("higher.[].low", 0);
    EXPECT_EQ(sameObjFromConfigData.getValue("admin").asBool(), firstObj.getValue("low.admin").asBool());
    EXPECT_FALSE(firstObj.containsKey("low"));
    EXPECT_TRUE(firstObj.containsKey("low.admin"));

    ObjectView const objLow = firstObj.getObject("low");
    EXPECT_TRUE(objLow.containsKey("section"));
    EXPECT_TRUE(objLow.containsKey("admin"));
    EXPECT_EQ(objLow.getValue("section").asString(), "true");
    EXPECT_EQ(objLow.getValue("admin").asBool(), false);
}

TEST_F(ObjectViewTest, getArrayInObject)
{
    auto const obj = configData.getObject("dosguard");
    EXPECT_TRUE(obj.containsKey("whitelist.[]"));

    auto const arr = obj.getArray("whitelist");
    EXPECT_EQ(2, arr.size());

    EXPECT_EQ("125.5.5.2", arr.valueAt(0).asString());
    EXPECT_EQ("204.2.2.2", arr.valueAt(1).asString());
}

struct ObjectViewDeathTest : ObjectViewTest {};

TEST_F(ObjectViewDeathTest, incorrectKeys)
{
    EXPECT_DEATH({ [[maybe_unused]] auto _ = configData.getObject("header.text1"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto _ = configData.getObject("head"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto _ = configData.getArray("header"); }, ".*");

    // dies because only 1 object in higher.[].low
    EXPECT_DEATH({ [[maybe_unused]] auto _ = configData.getObject("higher.[].low", 1); }, ".*");
}
