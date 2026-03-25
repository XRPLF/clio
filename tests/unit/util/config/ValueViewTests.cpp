#include "util/MockAssert.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/FakeConfigData.hpp"
#include "util/config/Types.hpp"
#include "util/config/ValueView.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

using namespace util::config;

struct ValueViewTest : virtual testing::Test {
    ClioConfigDefinition const configData = generateConfig();
};

TEST_F(ValueViewTest, ValueView)
{
    ConfigValue const cv = ConfigValue{ConfigType::String}.defaultValue("value");
    ValueView const vv = ValueView(cv);
    EXPECT_EQ("value", vv.asString());
    EXPECT_EQ(ConfigType::String, vv.type());
    EXPECT_EQ(true, vv.hasValue());
    EXPECT_EQ(false, vv.isOptional());
}

TEST_F(ValueViewTest, DifferentIntegerTest)
{
    auto const vv = configData.getValueView("header.port");
    auto const uint32 = vv.asIntType<uint32_t>();
    auto const uint64 = vv.asIntType<uint64_t>();
    auto const int32 = vv.asIntType<int32_t>();
    auto const int64 = vv.asIntType<int64_t>();

    EXPECT_EQ(vv.asIntType<int>(), uint32);
    EXPECT_EQ(vv.asIntType<int>(), uint64);
    EXPECT_EQ(vv.asIntType<int>(), int32);
    EXPECT_EQ(vv.asIntType<int>(), int64);

    auto const doubleVal = vv.asIntType<double>();
    auto const floatVal = vv.asIntType<float>();
    auto const sameDouble = vv.asDouble();
    auto const sameFloat = vv.asFloat();
    auto const precision = 1e-9;

    EXPECT_NEAR(doubleVal, sameDouble, precision);
    EXPECT_NEAR(floatVal, sameFloat, precision);

    auto const ipVal = configData.getValueView("ip");
    auto const ipDouble = ipVal.asDouble();
    auto const ipFloat = ipVal.asFloat();
    EXPECT_NEAR(ipDouble, 444.22, precision);
    EXPECT_NEAR(ipFloat, 444.22f, precision);
}

TEST_F(ValueViewTest, IntegerAsDoubleTypeValue)
{
    auto const cv =
        ConfigValue{ConfigType::Double}.defaultValue(432).withConstraint(gValidatePositiveDouble);
    ValueView const vv{cv};
    auto const doubleVal = vv.asFloat();
    auto const floatVal = vv.asDouble();

    auto const precision = 1e-9;
    EXPECT_NEAR(doubleVal, 432, precision);
    EXPECT_NEAR(floatVal, 432, precision);
}

TEST_F(ValueViewTest, OptionalValues)
{
    auto const cv = ConfigValue{ConfigType::Integer}.defaultValue(432).optional();
    auto const cv2 = ConfigValue{ConfigType::Double}.optional();
    auto const cv3 = ConfigValue{ConfigType::String}.optional();
    auto const cv4 = ConfigValue{ConfigType::String}.defaultValue("hello").optional();

    ValueView const vv{cv};
    ValueView const vv2{cv2};
    ValueView const vv3{cv3};
    ValueView const vv4{cv4};

    EXPECT_EQ(vv.asOptional<uint32_t>().value(), 432);
    EXPECT_EQ(vv.asOptional<uint64_t>().value(), 432);
    EXPECT_EQ(vv2.asOptional<uint64_t>(), std::nullopt);
    EXPECT_EQ(vv3.asOptional<std::string>(), std::nullopt);
    EXPECT_EQ(vv4.asOptional<std::string>(), "hello");
}

struct ValueViewAssertTest : common::util::WithMockAssert, ValueViewTest {};

TEST_F(ValueViewAssertTest, WrongTypes)
{
    auto const vv = configData.getValueView("header.port");
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = vv.asBool(); });
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = vv.asString(); });

    auto const cv = ConfigValue{ConfigType::Integer}.defaultValue(-5);
    auto const vv2 = ValueView(cv);
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = vv2.asIntType<uint32_t>(); });

    auto const cv2 = ConfigValue{ConfigType::String}.defaultValue("asdf");
    auto const vv3 = ValueView(cv2);
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = vv3.asDouble(); });
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = vv3.asFloat(); });
}
