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

#include "util/LoggerFixtures.hpp"
#include "util/MockAssert.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Error.hpp"
#include "util/config/Types.hpp"

#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <ostream>
#include <string>

using namespace util::config;

struct ConfigValueTest : common::util::WithMockAssert, NoLoggerFixture {};

TEST_F(ConfigValueTest, construct)
{
    ConfigValue const cv{ConfigType::Integer};
    EXPECT_FALSE(cv.hasValue());
    EXPECT_FALSE(cv.isOptional());
    EXPECT_EQ(cv.type(), ConfigType::Integer);
}

TEST_F(ConfigValueTest, optional)
{
    auto const cv = ConfigValue{ConfigType::Integer}.optional();
    EXPECT_FALSE(cv.hasValue());
    EXPECT_TRUE(cv.isOptional());
    EXPECT_EQ(cv.type(), ConfigType::Integer);
}

TEST_F(ConfigValueTest, defaultValue)
{
    auto const cv = ConfigValue{ConfigType::Integer}.defaultValue(123);
    EXPECT_TRUE(cv.hasValue());
    EXPECT_FALSE(cv.isOptional());
    EXPECT_EQ(cv.type(), ConfigType::Integer);
}

TEST_F(ConfigValueTest, defaultValueWithDescription)
{
    auto const cv = ConfigValue{ConfigType::String}.defaultValue("123", "random description");
    EXPECT_TRUE(cv.hasValue());
    EXPECT_FALSE(cv.isOptional());
    EXPECT_EQ(cv.type(), ConfigType::String);
}

TEST_F(ConfigValueTest, invalidDefaultValue)
{
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto const a = ConfigValue{ConfigType::String}.defaultValue(33); });
}

TEST_F(ConfigValueTest, setValueWrongType)
{
    auto cv = ConfigValue{ConfigType::Integer};
    auto const err = cv.setValue("123");
    EXPECT_TRUE(err.has_value());
}

TEST_F(ConfigValueTest, setValueNormalPath)
{
    auto cv = ConfigValue{ConfigType::Integer};
    auto const err = cv.setValue(123);
    EXPECT_FALSE(err.has_value());
    EXPECT_EQ(cv.getValue(), Value{123});
}

struct ConfigValueConstraintTest : ConfigValueTest {
    struct MockConstraint : Constraint {
        MOCK_METHOD(std::optional<Error>, checkTypeImpl, (Value const&), (const, override));
        MOCK_METHOD(std::optional<Error>, checkValueImpl, (Value const&), (const, override));
        MOCK_METHOD(void, print, (std::ostream&), (const, override));
    };

    testing::StrictMock<MockConstraint> constraint;
};

TEST_F(ConfigValueConstraintTest, setValueWithConstraint)
{
    auto cv = ConfigValue{ConfigType::Integer}.withConstraint(constraint);
    auto const value = Value{123};
    EXPECT_CALL(constraint, checkTypeImpl).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(constraint, checkValueImpl).WillOnce(testing::Return(std::nullopt));
    auto const err = cv.setValue(value);
    EXPECT_FALSE(err.has_value());
    EXPECT_EQ(cv.getValue(), value);
}

TEST_F(ConfigValueConstraintTest, setValueWithConstraintTypeCheckError)
{
    auto cv = ConfigValue{ConfigType::Integer}.withConstraint(constraint);
    auto const value = 123;
    EXPECT_CALL(constraint, checkTypeImpl).WillOnce(testing::Return(Error{"type error"}));
    auto const err = cv.setValue(value);
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(err->error, "Unknown_key type error");
}

TEST_F(ConfigValueConstraintTest, defaultValueWithConstraint)
{
    EXPECT_CALL(constraint, checkTypeImpl).WillOnce(testing::Return(std::nullopt));
    EXPECT_CALL(constraint, checkValueImpl).WillOnce(testing::Return(std::nullopt));
    auto const cv = ConfigValue{ConfigType::Integer}.defaultValue(123).withConstraint(constraint);
    EXPECT_EQ(cv.getValue(), Value{123});
}

TEST_F(ConfigValueConstraintTest, defaultValueWithConstraintCheckError)
{
    EXPECT_CLIO_ASSERT_FAIL({
        EXPECT_CALL(constraint, checkTypeImpl).WillOnce(testing::Return(std::nullopt));
        EXPECT_CALL(constraint, checkValueImpl).WillOnce(testing::Return(Error{"value error"}));
        [[maybe_unused]] auto const cv = ConfigValue{ConfigType::Integer}.defaultValue(123).withConstraint(constraint);
    });
}

// A test for each constraint so it's easy to change in the future
struct ConstraintTest : NoLoggerFixture {};

TEST_F(ConstraintTest, PortConstraint)
{
    auto const portConstraint{PortConstraint{}};
    EXPECT_FALSE(portConstraint.checkConstraint(4444).has_value());
    EXPECT_TRUE(portConstraint.checkConstraint(99999).has_value());
}

TEST_F(ConstraintTest, SetValuesOnPortConstraint)
{
    auto cvPort = ConfigValue{ConfigType::Integer}.defaultValue(4444).withConstraint(gValidatePort);
    auto const err = cvPort.setValue(99999);
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(err->error, "Unknown_key Port does not satisfy the constraint bounds");
    EXPECT_TRUE(cvPort.setValue(33.33).has_value());
    EXPECT_EQ(cvPort.setValue(33.33).value().error, "Unknown_key value does not match type integer");
    EXPECT_FALSE(cvPort.setValue(1).has_value());

    auto cvPort2 = ConfigValue{ConfigType::String}.defaultValue("4444").withConstraint(gValidatePort);
    auto const strPortError = cvPort2.setValue("100000");
    EXPECT_TRUE(strPortError.has_value());
    EXPECT_EQ(strPortError->error, "Unknown_key Port does not satisfy the constraint bounds");
}

TEST_F(ConstraintTest, OneOfConstraintOneValue)
{
    std::array<char const*, 1> const arr = {"tracer"};
    auto const databaseConstraint{OneOf{"database.type", arr}};
    EXPECT_FALSE(databaseConstraint.checkConstraint("tracer").has_value());

    EXPECT_TRUE(databaseConstraint.checkConstraint(345).has_value());
    EXPECT_EQ(databaseConstraint.checkConstraint(345)->error, R"(Key "database.type"'s value must be a string)");

    EXPECT_TRUE(databaseConstraint.checkConstraint("123.44").has_value());
    EXPECT_EQ(
        databaseConstraint.checkConstraint("123.44")->error,
        R"(You provided value "123.44". Key "database.type"'s value must be one of the following: tracer)"
    );
}

TEST_F(ConstraintTest, OneOfConstraint)
{
    std::array<char const*, 3> const arr = {"123", "trace", "haha"};
    auto const oneOfCons{OneOf{"log_level", arr}};

    EXPECT_FALSE(oneOfCons.checkConstraint("trace").has_value());

    EXPECT_TRUE(oneOfCons.checkConstraint(345).has_value());
    EXPECT_EQ(oneOfCons.checkConstraint(345)->error, R"(Key "log_level"'s value must be a string)");

    EXPECT_TRUE(oneOfCons.checkConstraint("PETER_WAS_HERE").has_value());
    EXPECT_EQ(
        oneOfCons.checkConstraint("PETER_WAS_HERE")->error,
        R"(You provided value "PETER_WAS_HERE". Key "log_level"'s value must be one of the following: 123, trace, haha)"
    );
}

TEST_F(ConstraintTest, IpConstraint)
{
    auto ip = ConfigValue{ConfigType::String}.withConstraint(gValidateIp);

    EXPECT_FALSE(ip.setValue("127.0.0.1"));
    EXPECT_FALSE(ip.setValue("localhost"));
    EXPECT_FALSE(ip.setValue("some-server.com"));
    EXPECT_FALSE(ip.setValue("126.0.0.2"));
    EXPECT_FALSE(ip.setValue("valid.host-name.com"));

    EXPECT_TRUE(ip.setValue("http://127.0.0.1"));
    EXPECT_TRUE(ip.setValue(""));
    EXPECT_TRUE(ip.setValue("example..com"));
    EXPECT_TRUE(ip.setValue("example.com:8080/path"));
    EXPECT_TRUE(ip.setValue("-invalid.com"));
    EXPECT_TRUE(ip.setValue("invalid-.com"));

    auto const err = ip.setValue("extra$@symbols");
    ASSERT_TRUE(err.has_value());
    EXPECT_EQ(err->error, "Unknown_key Ip is not a valid ip address or hostname");
}

TEST_F(ConstraintTest, positiveNumConstraint)
{
    auto const numCons{NumberValueConstraint{0, 5}};
    EXPECT_FALSE(numCons.checkConstraint(0));
    EXPECT_FALSE(numCons.checkConstraint(5));

    EXPECT_TRUE(numCons.checkConstraint(true));
    EXPECT_EQ(numCons.checkConstraint(true)->error, fmt::format("Number must be of type integer"));

    EXPECT_TRUE(numCons.checkConstraint(8));
    EXPECT_EQ(numCons.checkConstraint(8)->error, fmt::format("Number must be between {} and {}", 0, 5));
}

TEST_F(ConstraintTest, SetValuesOnNumberConstraint)
{
    auto positiveNum = ConfigValue{ConfigType::Integer}.defaultValue(20u).withConstraint(gValidateUint16);
    auto const err = positiveNum.setValue(-22, "key");
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(err->error, fmt::format("key Number must be between {} and {}", 1, 65535));
    EXPECT_FALSE(positiveNum.setValue(99, "key"));
}

TEST_F(ConstraintTest, PositiveDoubleConstraint)
{
    auto const doubleCons{PositiveDouble{}};
    EXPECT_FALSE(doubleCons.checkConstraint(0.2));
    EXPECT_FALSE(doubleCons.checkConstraint(5.54));
    EXPECT_FALSE(doubleCons.checkConstraint(3));
    EXPECT_TRUE(doubleCons.checkConstraint("-5"));
    EXPECT_EQ(doubleCons.checkConstraint("-5")->error, "Double number must be of type int or double");
    EXPECT_EQ(doubleCons.checkConstraint(-5.6)->error, "Double number must be greater than or equal to 0");
    EXPECT_FALSE(doubleCons.checkConstraint(12.1));
}

struct ConstraintTestBundle {
    std::string name;
    Constraint const& constraint;
};

struct ConstraintAssertTest : common::util::WithMockAssert, testing::WithParamInterface<ConstraintTestBundle> {};

INSTANTIATE_TEST_SUITE_P(
    EachConstraints,
    ConstraintAssertTest,
    testing::Values(
        ConstraintTestBundle{"logTagConstraint", gValidateLogTag},
        ConstraintTestBundle{"portConstraint", gValidatePort},
        ConstraintTestBundle{"ipConstraint", gValidateIp},
        ConstraintTestBundle{"channelConstraint", gValidateChannelName},
        ConstraintTestBundle{"logLevelConstraint", gValidateLogLevelName},
        ConstraintTestBundle{"cassandraNameConstraint", gValidateCassandraName},
        ConstraintTestBundle{"loadModeConstraint", gValidateLoadMode},
        ConstraintTestBundle{"ChannelNameConstraint", gValidateChannelName},
        ConstraintTestBundle{"ApiVersionConstraint", gValidateApiVersion},
        ConstraintTestBundle{"Uint16Constraint", gValidateUint16},
        ConstraintTestBundle{"Uint32Constraint", gValidateUint32},
        ConstraintTestBundle{"PositiveDoubleConstraint", gValidatePositiveDouble}
    ),
    [](testing::TestParamInfo<ConstraintTestBundle> const& info) { return info.param.name; }
);

TEST_P(ConstraintAssertTest, TestEachConstraint)
{
    EXPECT_CLIO_ASSERT_FAIL({
        [[maybe_unused]] auto const a =
            ConfigValue{ConfigType::Boolean}.defaultValue(true).withConstraint(GetParam().constraint);
    });
}

TEST_F(ConstraintAssertTest, SetInvalidValueTypeStringAndBool)
{
    EXPECT_CLIO_ASSERT_FAIL({
        [[maybe_unused]] auto a = ConfigValue{ConfigType::String}.defaultValue(33).withConstraint(gValidateLoadMode);
    });
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto a = ConfigValue{ConfigType::Boolean}.defaultValue(-66); });
}

TEST_F(ConstraintAssertTest, OutOfBounceIntegerConstraint)
{
    EXPECT_CLIO_ASSERT_FAIL({
        [[maybe_unused]] auto a = ConfigValue{ConfigType::Integer}.defaultValue(999999).withConstraint(gValidateUint16);
    });
    EXPECT_CLIO_ASSERT_FAIL({
        [[maybe_unused]] auto a = ConfigValue{ConfigType::Integer}.defaultValue(-66).withConstraint(gValidateUint32);
    });
}
