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

#include <../common/newconfig/FakeConfigData.hpp>
#include <gtest/gtest.h>

using namespace util::config;

struct ObjectViewTest : testing::Test {
    ClioConfigDefinition configData = generateConfig();
};

TEST_F(ObjectViewTest, ObjectValueTest)
{
    auto headerObj = configData.getObject("header");
    ASSERT_FALSE(headerObj.containsKey("header"));
    ASSERT_TRUE(headerObj.containsKey("text1"));
    ASSERT_TRUE(headerObj.containsKey("port"));
    ASSERT_TRUE(headerObj.containsKey("admin"));

    ASSERT_EQ("value", headerObj.getValue("text1").asString());
    ASSERT_EQ(123, headerObj.getValue("port").asInt());
    ASSERT_EQ(true, headerObj.getValue("admin").asBool());
}

TEST_F(ObjectViewTest, ObjectInArray)
{
    ArrayView arr = configData.getArray("array");
    ASSERT_EQ(arr.size(), 2);

    ObjectView firstObj = arr.objectAt(0);
    ObjectView secondObj = arr.objectAt(1);

    ASSERT_TRUE(firstObj.containsKey("sub"));
    ASSERT_TRUE(firstObj.containsKey("sub2"));

    // object's key is only "sub" and "sub2"
    ASSERT_FALSE(firstObj.containsKey("array.[].sub"));

    ASSERT_EQ(firstObj.getValue("sub").asDouble(), 111.11);
    ASSERT_EQ(firstObj.getValue("sub2").asString(), "subCategory");

    ASSERT_EQ(secondObj.getValue("sub").asDouble(), 4321.55);
    ASSERT_EQ(secondObj.getValue("sub2").asString(), "temporary");
}

TEST_F(ObjectViewTest, ObjectInArrayMoreComplex)
{
    ArrayView arr = configData.getArray("higher");
    ASSERT_EQ(1, arr.size());

    ObjectView firstObj = arr.objectAt(0);

    // this returns the 1st object inside "low"
    ObjectView sameObjFromConfigData = configData.getObject("higher.[].low", 0);
    ASSERT_EQ(sameObjFromConfigData.getValue("admin").asBool(), firstObj.getValue("low.admin").asBool());
    ASSERT_FALSE(firstObj.containsKey("low"));
    ASSERT_TRUE(firstObj.containsKey("low.admin"));

    ObjectView objLow = firstObj.getObject("low");
    ASSERT_TRUE(objLow.containsKey("section"));
    ASSERT_TRUE(objLow.containsKey("admin"));
    ASSERT_EQ(objLow.getValue("section").asString(), "true");
    ASSERT_EQ(objLow.getValue("admin").asBool(), false);
}
