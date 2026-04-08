#include "util/MockAssert.hpp"
#include "util/config/ArrayView.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/FakeConfigData.hpp"
#include "util/config/ObjectView.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace util::config;

struct ObjectViewTest : virtual testing::Test {
    ObjectViewTest()
    {
        ConfigFileJson const jsonFileObj{boost::json::parse(kJSON_DATA).as_object()};
        auto const errors = configData.parse(jsonFileObj);
        EXPECT_TRUE(!errors.has_value());
    }
    ClioConfigDefinition configData = generateConfig();
};

TEST_F(ObjectViewTest, ObjectContainsKeyTest)
{
    auto const headerObj = configData.getObject("header");
    EXPECT_FALSE(headerObj.containsKey("header"));
    EXPECT_TRUE(headerObj.containsKey("text1"));
    EXPECT_TRUE(headerObj.containsKey("port"));
    EXPECT_TRUE(headerObj.containsKey("admin"));
}

TEST_F(ObjectViewTest, ObjectValueTest)
{
    auto const headerObj = configData.getObject("header");
    EXPECT_EQ("value", headerObj.getValueView("text1").asString());
    EXPECT_EQ(321, headerObj.getValueView("port").asIntType<int>());
    EXPECT_EQ(false, headerObj.getValueView("admin").asBool());
}

TEST_F(ObjectViewTest, ObjectGetValueByTemplateTest)
{
    auto const headerObj = configData.getObject("header");
    EXPECT_EQ("value", headerObj.get<std::string>("text1"));
    EXPECT_EQ(321, headerObj.get<int>("port"));
    EXPECT_EQ(false, headerObj.get<bool>("admin"));
}

TEST_F(ObjectViewTest, GetOptionalValue)
{
    auto const optionalObj = configData.getObject("optional");
    EXPECT_EQ(std::nullopt, optionalObj.maybeValue<double>("withNoDefault"));
    EXPECT_EQ(0.0, optionalObj.maybeValue<double>("withDefault"));
}

TEST_F(ObjectViewTest, ObjectValuesInArray)
{
    ArrayView const arr = configData.getArray("array");
    EXPECT_EQ(arr.size(), 3);
    ObjectView const firstObj = arr.objectAt(0);
    ObjectView const secondObj = arr.objectAt(1);
    EXPECT_TRUE(firstObj.containsKey("sub"));
    EXPECT_TRUE(firstObj.containsKey("sub2"));

    // object's key is only "sub" and "sub2"
    EXPECT_FALSE(firstObj.containsKey("array.[].sub"));

    EXPECT_EQ(firstObj.getValueView("sub").asDouble(), 111.11);
    EXPECT_EQ(firstObj.getValueView("sub2").asString(), "subCategory");

    EXPECT_EQ(secondObj.getValueView("sub").asDouble(), 4321.55);
    EXPECT_EQ(secondObj.getValueView("sub2").asString(), "temporary");
}

TEST_F(ObjectViewTest, GetObjectsInDifferentWays)
{
    ArrayView const arr = configData.getArray("higher");
    ASSERT_EQ(1, arr.size());

    ObjectView const firstObj = arr.objectAt(0);

    // this returns the 1st object inside "low"
    ObjectView const sameObjFromConfigData = configData.getObject("higher.[].low", 0);
    EXPECT_EQ(
        sameObjFromConfigData.getValueView("admin").asBool(),
        firstObj.getValueView("low.admin").asBool()
    );
    EXPECT_FALSE(firstObj.containsKey("low"));
    EXPECT_TRUE(firstObj.containsKey("low.admin"));

    ObjectView const objLow = firstObj.getObject("low");
    EXPECT_TRUE(objLow.containsKey("section"));
    EXPECT_TRUE(objLow.containsKey("admin"));
    EXPECT_EQ(objLow.getValueView("section").asString(), "WebServer");
    EXPECT_EQ(objLow.getValueView("admin").asBool(), false);
}

TEST_F(ObjectViewTest, getArrayInObject)
{
    auto const obj = configData.getObject("dosguard");
    EXPECT_TRUE(obj.containsKey("whitelist.[]"));

    auto const arr = obj.getArray("whitelist");
    EXPECT_EQ(2, arr.size());

    EXPECT_EQ("125.5.5.1", arr.valueAt(0).asString());
    EXPECT_EQ("204.2.2.1", arr.valueAt(1).asString());
}

struct ObjectViewAssertTest : common::util::WithMockAssert, ObjectViewTest {};

TEST_F(ObjectViewAssertTest, KeyDoesNotExist)
{
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto _ = configData.getObject("head"); });
}

TEST_F(ObjectViewAssertTest, KeyIsValueView)
{
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto _ = configData.getObject("header.text1"); });
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto _ = configData.getArray("header"); });
}

TEST_F(ObjectViewAssertTest, KeyisArrayView)
{
    // dies because only 1 object in higher.[].low
    EXPECT_CLIO_ASSERT_FAIL({
        [[maybe_unused]] auto _ = configData.getObject("higher.[].low", 1);
    });
}

TEST_F(ObjectViewAssertTest, KeyisNotOptional)
{
    // dies because not an optional
    EXPECT_CLIO_ASSERT_FAIL({
        [[maybe_unused]] auto _ = configData.getObject("header").maybeValue<std::string>("text1");
    });
}
