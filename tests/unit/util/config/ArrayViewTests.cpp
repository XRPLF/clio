#include "util/MockAssert.hpp"
#include "util/config/ArrayView.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/FakeConfigData.hpp"
#include "util/config/ObjectView.hpp"
#include "util/config/Types.hpp"
#include "util/config/ValueView.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

#include <cstddef>
#include <string>

using namespace util::config;

struct ArrayViewTest : virtual testing::Test {
    ArrayViewTest()
    {
        ConfigFileJson const jsonFileObj{boost::json::parse(kJsonData).as_object()};
        auto const errors = configData.parse(jsonFileObj);
        EXPECT_TRUE(!errors.has_value());
    }
    ClioConfigDefinition configData = generateConfig();
};

// Array View tests can only be tested after the values are populated from user Config
// into ConfigClioDefinition
TEST_F(ArrayViewTest, ArrayGetValueDouble)
{
    auto const precision = 1e-9;
    ArrayView const arrVals = configData.getArray("array.[].sub");

    auto const firstVal = arrVals.valueAt(0);
    EXPECT_EQ(firstVal.type(), ConfigType::Double);
    EXPECT_TRUE(firstVal.hasValue());
    EXPECT_FALSE(firstVal.isOptional());

    EXPECT_NEAR(111.11, firstVal.asDouble(), precision);
    EXPECT_NEAR(4321.55, arrVals.valueAt(1).asDouble(), precision);
}

TEST_F(ArrayViewTest, ArrayGetValueString)
{
    ArrayView const arrVals = configData.getArray("array.[].sub2");
    ValueView const firstVal = arrVals.valueAt(0);

    EXPECT_EQ(firstVal.type(), ConfigType::String);
    EXPECT_EQ("subCategory", firstVal.asString());
    EXPECT_EQ("london", arrVals.valueAt(2).asString());
}

TEST_F(ArrayViewTest, IterateValuesDouble)
{
    auto const precision = 1e-9;
    ArrayView const arrVals = configData.getArray("array.[].sub");

    auto valIt = arrVals.begin<ValueView>();
    EXPECT_NEAR((*valIt++).asDouble(), 111.11, precision);
    EXPECT_NEAR((*valIt++).asDouble(), 4321.55, precision);
    EXPECT_NEAR((*valIt++).asDouble(), 5555.44, precision);
    EXPECT_EQ(valIt, arrVals.end<ValueView>());
}

TEST_F(ArrayViewTest, IterateValuesString)
{
    ArrayView const arrVals = configData.getArray("array.[].sub2");

    auto val2It = arrVals.begin<ValueView>();
    EXPECT_EQ((*val2It++).asString(), "subCategory");
    EXPECT_EQ((*val2It++).asString(), "temporary");
    EXPECT_EQ((*val2It++).asString(), "london");
    EXPECT_EQ(val2It, arrVals.end<ValueView>());
}

TEST_F(ArrayViewTest, ArrayWithObj)
{
    ArrayView const arrVals = configData.getArray("array.[]");
    ArrayView const arrValAlt = configData.getArray("array");
    auto const precision = 1e-9;

    auto const obj1 = arrVals.objectAt(0);
    auto const obj2 = arrValAlt.objectAt(0);
    EXPECT_NEAR(obj1.get<double>("sub"), obj2.get<double>("sub"), precision);
    EXPECT_NEAR(obj1.get<double>("sub"), 111.11, precision);
}

TEST_F(ArrayViewTest, IterateArray)
{
    auto arr = configData.getArray("dosguard.whitelist");
    EXPECT_EQ(2, arr.size());
    EXPECT_EQ(arr.valueAt(0).asString(), "125.5.5.1");
    EXPECT_EQ(arr.valueAt(1).asString(), "204.2.2.1");

    auto it = arr.begin<ValueView>();
    EXPECT_EQ((*it++).asString(), "125.5.5.1");
    EXPECT_EQ((*it++).asString(), "204.2.2.1");
    EXPECT_EQ((it), arr.end<ValueView>());
}

TEST_F(ArrayViewTest, CompareDifferentArrayIterators)
{
    auto const subArray = configData.getArray("array.[].sub");
    auto const dosguardArray = configData.getArray("dosguard.whitelist.[]");

    auto itArray = subArray.begin<ValueView>();
    auto itDosguard = dosguardArray.begin<ValueView>();

    for (std::size_t i = 0; i < subArray.size(); i++)
        EXPECT_NE(itArray++, itDosguard++);
}

TEST_F(ArrayViewTest, IterateObject)
{
    auto arr = configData.getArray("array");
    EXPECT_EQ(3, arr.size());

    auto it = arr.begin<ObjectView>();
    EXPECT_EQ(111.11, (*it).get<double>("sub"));
    EXPECT_EQ("subCategory", (*it++).get<std::string>("sub2"));

    EXPECT_EQ(4321.55, (*it).get<double>("sub"));
    EXPECT_EQ("temporary", (*it++).get<std::string>("sub2"));

    EXPECT_EQ(5555.44, (*it).get<double>("sub"));
    EXPECT_EQ("london", (*it++).get<std::string>("sub2"));

    EXPECT_EQ(it, arr.end<ObjectView>());
}

struct ArrayViewAssertTest : common::util::WithMockAssert, ArrayViewTest {};

TEST_F(ArrayViewAssertTest, AccessArrayOutOfBounce)
{
    // dies because higher only has 1 object (trying to access 2nd element)
    EXPECT_CLIO_ASSERT_FAIL({
        [[maybe_unused]] auto _ = configData.getArray("higher").objectAt(1);
    });
}

TEST_F(ArrayViewAssertTest, AccessIndexOfWrongType)
{
    auto const& arrVals2 = configData.getArray("array.[].sub2");
    auto const& tempVal = arrVals2.valueAt(0);

    // dies as value is not of type int
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto _ = tempVal.asIntType<int>(); });
}

TEST_F(ArrayViewAssertTest, GetValueWhenItIsObject)
{
    ArrayView const arr = configData.getArray("higher");
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto _ = arr.begin<ValueView>(); });
}

TEST_F(ArrayViewAssertTest, GetObjectWhenItIsValue)
{
    ArrayView const dosguardWhitelist = configData.getArray("dosguard.whitelist");
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto _ = dosguardWhitelist.begin<ObjectView>(); });
}
