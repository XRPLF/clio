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

#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <fmt/core.h>
#include <gtest/gtest.h>

#include <array>
#include <string>

using namespace util::config;

TEST(ConfigValue, GetSetString)
{
    auto const cvStr = ConfigValue{ConfigType::String}.defaultValue("12345");
    EXPECT_EQ(cvStr.type(), ConfigType::String);
    EXPECT_TRUE(cvStr.hasValue());
    EXPECT_FALSE(cvStr.isOptional());
}

TEST(ConfigValue, GetSetInteger)
{
    auto const cvInt = ConfigValue{ConfigType::Integer}.defaultValue(543);
    EXPECT_EQ(cvInt.type(), ConfigType::Integer);
    EXPECT_TRUE(cvInt.hasValue());
    EXPECT_FALSE(cvInt.isOptional());

    auto const cvOpt = ConfigValue{ConfigType::Integer}.optional();
    EXPECT_TRUE(cvOpt.isOptional());
}

// A test for each constraint so it's easy to change in the future
TEST(ConfigValue, PortConstraint)
{
    auto const portConstraint{PortConstraint{}};
    EXPECT_FALSE(portConstraint.checkConstraint(4444).has_value());
    EXPECT_TRUE(portConstraint.checkConstraint(99999).has_value());
}

TEST(ConfigValue, SetValuesOnPortConstraint)
{
    auto cvPort = ConfigValue{ConfigType::Integer}.defaultValue(4444).withConstraint(validatePort);
    auto const err = cvPort.setValue(99999);
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(err->error, "Port does not satisfy the constraint bounds");
    EXPECT_TRUE(cvPort.setValue(33.33).has_value());
    EXPECT_TRUE(cvPort.setValue(33.33).value().error == "value does not match type integer");
    EXPECT_FALSE(cvPort.setValue(1).has_value());

    auto cvPort2 = ConfigValue{ConfigType::String}.defaultValue("4444").withConstraint(validatePort);
    auto const strPortError = cvPort2.setValue("100000");
    EXPECT_TRUE(strPortError.has_value());
    EXPECT_EQ(strPortError->error, "Port does not satisfy the constraint bounds");
}

TEST(ConfigValue, OneOfConstraintOneValue)
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

TEST(ConfigValue, OneOfConstraint)
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

TEST(ConfigValue, IpConstraint)
{
    auto ip = ConfigValue{ConfigType::String}.defaultValue("127.0.0.1").withConstraint(validateIP);
    EXPECT_FALSE(ip.setValue("http://127.0.0.1").has_value());
    EXPECT_FALSE(ip.setValue("http://127.0.0.1.com").has_value());
    auto const err = ip.setValue("123.44");
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(err->error, "Ip is not a valid ip address");
    EXPECT_FALSE(ip.setValue("126.0.0.2"));

    EXPECT_TRUE(ip.setValue("644.3.3.0"));
    EXPECT_TRUE(ip.setValue("127.0.0.1.0"));
    EXPECT_TRUE(ip.setValue(""));
    EXPECT_TRUE(ip.setValue("http://example..com"));
    EXPECT_FALSE(ip.setValue("localhost"));
    EXPECT_FALSE(ip.setValue("http://example.com:8080/path"));
}

TEST(ConfigValue, positiveNumConstraint)
{
    auto const numCons{NumberValueConstraint{0, 5}};
    EXPECT_FALSE(numCons.checkConstraint(0));
    EXPECT_FALSE(numCons.checkConstraint(5));

    EXPECT_TRUE(numCons.checkConstraint(true));
    EXPECT_EQ(numCons.checkConstraint(true)->error, fmt::format("Number must be of type integer"));

    EXPECT_TRUE(numCons.checkConstraint(8));
    EXPECT_EQ(numCons.checkConstraint(8)->error, fmt::format("Number must be between {} and {}", 0, 5));
}

TEST(ConfigValue, SetValuesOnNumberConstraint)
{
    auto positiveNum = ConfigValue{ConfigType::Integer}.defaultValue(20u).withConstraint(validateUint16);
    auto const err = positiveNum.setValue(-22, "key");
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(err->error, fmt::format("key Number must be between {} and {}", 0, 65535));
    EXPECT_FALSE(positiveNum.setValue(99, "key"));
}

TEST(ConfigValue, PositiveDoubleConstraint)
{
    auto const doubleCons{PositiveDouble{}};
    EXPECT_FALSE(doubleCons.checkConstraint(0.2));
    EXPECT_FALSE(doubleCons.checkConstraint(5.54));
    EXPECT_TRUE(doubleCons.checkConstraint("-5"));
    EXPECT_EQ(doubleCons.checkConstraint("-5")->error, "Double number must be of type int or double");
    EXPECT_EQ(doubleCons.checkConstraint(-5.6)->error, "Double number must be greater than 0");
    EXPECT_FALSE(doubleCons.checkConstraint(12.1));
}

struct ConstraintTestBundle {
    std::string name;
    Constraint const& cons_;
};

struct ConstraintDeathTest : public testing::Test, public testing::WithParamInterface<ConstraintTestBundle> {};

INSTANTIATE_TEST_SUITE_P(
    EachConstraints,
    ConstraintDeathTest,
    testing::Values(
        ConstraintTestBundle{"logTagConstraint", validateLogTag},
        ConstraintTestBundle{"portConstraint", validatePort},
        ConstraintTestBundle{"ipConstraint", validateIP},
        ConstraintTestBundle{"channelConstraint", validateChannelName},
        ConstraintTestBundle{"logLevelConstraint", validateLogLevelName},
        ConstraintTestBundle{"cannsandraNameCnstraint", validateCassandraName},
        ConstraintTestBundle{"loadModeConstraint", validateLoadMode},
        ConstraintTestBundle{"ChannelNameConstraint", validateChannelName},
        ConstraintTestBundle{"ApiVersionConstraint", validateApiVersion},
        ConstraintTestBundle{"Uint16Constraint", validateUint16},
        ConstraintTestBundle{"Uint32Constraint", validateUint32},
        ConstraintTestBundle{"PositiveDoubleConstraint", validatePositiveDouble}
    ),
    [](testing::TestParamInfo<ConstraintTestBundle> const& info) { return info.param.name; }
);

TEST_P(ConstraintDeathTest, TestEachConstraint)
{
    EXPECT_DEATH(
        {
            [[maybe_unused]] auto const a =
                ConfigValue{ConfigType::Boolean}.defaultValue(true).withConstraint(GetParam().cons_);
        },
        ".*"
    );
}

TEST(ConfigValueDeathTest, SetInvalidValueTypeStringAndBool)
{
    EXPECT_DEATH(
        {
            [[maybe_unused]] auto a = ConfigValue{ConfigType::String}.defaultValue(33).withConstraint(validateLoadMode);
        },
        ".*"
    );
    EXPECT_DEATH({ [[maybe_unused]] auto a = ConfigValue{ConfigType::Boolean}.defaultValue(-66); }, ".*");
}

TEST(ConfigValueDeathTest, OutOfBounceIntegerConstraint)
{
    EXPECT_DEATH(
        {
            [[maybe_unused]] auto a =
                ConfigValue{ConfigType::Integer}.defaultValue(999999).withConstraint(validateUint16);
        },
        ".*"
    );
    EXPECT_DEATH(
        {
            [[maybe_unused]] auto a = ConfigValue{ConfigType::Integer}.defaultValue(-66).withConstraint(validateUint32);
        },
        ".*"
    );
}
