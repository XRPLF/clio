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

#include "rpc/common/APIVersion.hpp"
#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <fmt/core.h>
#include <gtest/gtest.h>

using namespace util::config;

TEST(ConfigValue, testConfigValue)
{
    auto const cvStr = ConfigValue{ConfigType::String}.defaultValue("12345");
    EXPECT_EQ(cvStr.type(), ConfigType::String);
    EXPECT_TRUE(cvStr.hasValue());
    EXPECT_FALSE(cvStr.isOptional());

    auto const cvInt = ConfigValue{ConfigType::Integer}.defaultValue(543);
    EXPECT_EQ(cvInt.type(), ConfigType::Integer);
    EXPECT_TRUE(cvStr.hasValue());
    EXPECT_FALSE(cvStr.isOptional());

    auto const cvOpt = ConfigValue{ConfigType::Integer}.optional();
    EXPECT_TRUE(cvOpt.isOptional());
}

// A test for each constraint so it's easy to change in the future
TEST(ConfigValue, portConstraint)
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

TEST(ConfigValue, channelConstraint)
{
    auto cvChannel = ConfigValue{ConfigType::String}.defaultValue("General").withConstraint(validateChannelName);
    auto const err = cvChannel.setValue("nono");
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(
        err->error,
        "Key \"channel\"'s value must be one of the following: General, WebServer, Backend, RPC, ETL, Subscriptions, "
        "Performance"
    );
    EXPECT_FALSE(cvChannel.setValue("Performance"));
}

TEST(ConfigValue, logLevelConstraint)
{
    auto logLevelChannel = ConfigValue{ConfigType::String}.defaultValue("info").withConstraint(validateLogLevelName);
    auto const err = logLevelChannel.setValue("PETER_WAS_HERE");
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(
        err->error,
        "Key \"log_level\"'s value must be one of the following: trace, debug, info, warning, error, fatal, count"
    );
    EXPECT_FALSE(logLevelChannel.setValue("count"));
}

TEST(ConfigValue, ipConstraint)
{
    auto ip = ConfigValue{ConfigType::String}.defaultValue("127.0.0.1").withConstraint(validateIP);
    EXPECT_FALSE(ip.setValue("http://127.0.0.1").has_value());
    EXPECT_FALSE(ip.setValue("http://127.0.0.1.com").has_value());
    auto const err4 = ip.setValue("123.44");
    EXPECT_TRUE(err4.has_value());
    EXPECT_EQ(err4->error, "ip is not a valid ip address");
    EXPECT_FALSE(ip.setValue("126.0.0.2"));
}

TEST(ConfigValue, databaseTypeConstraint)
{
    auto cassandra = ConfigValue{ConfigType::String}.defaultValue("cassandra").withConstraint(validateCassandraName);
    auto const casError = cassandra.setValue("123.44");
    EXPECT_TRUE(casError.has_value());
    EXPECT_EQ(casError->error, "Key \"database.type\"'s value must be string Cassandra");
    EXPECT_FALSE(cassandra.setValue("cassandra"));
}

TEST(ConfigValue, cacheLoadConstraint)
{
    auto load = ConfigValue{ConfigType::String}.defaultValue("async").withConstraint(validateLoadMode);
    auto const err = load.setValue("ASYCS");
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(err->error, "Key \"cache.load\"'s value must be one of the following: sync, async, none");
    EXPECT_FALSE(load.setValue("none"));
    EXPECT_FALSE(load.setValue("sync"));
}

TEST(ConfigValue, logTagStyleConstraint)
{
    auto logTagName = ConfigValue{ConfigType::String}.defaultValue("uint").withConstraint(validateLogTag);
    auto const err = logTagName.setValue("idek_anymore");
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(err->error, "Key \"log_tag_style\"'s value must be one of the following: int, uint, null, none, uuid");
    EXPECT_FALSE(logTagName.setValue("null"));
    EXPECT_FALSE(logTagName.setValue("uuid"));
}

TEST(ConfigValue, apiVersionConstraint)
{
    auto apiVer = ConfigValue{ConfigType::Integer}.defaultValue(1u).withConstraint(validateApiVersion);
    auto const err = apiVer.setValue(9999);
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(
        err->error, fmt::format("api_version must be between {} and {}", rpc::API_VERSION_MIN, rpc::API_VERSION_MAX)
    );
    EXPECT_FALSE(apiVer.setValue(rpc::API_VERSION_MAX));
    auto const apiWrongType = apiVer.setValue("9");
    EXPECT_TRUE(apiWrongType.has_value());
    EXPECT_EQ(apiWrongType->error, "value does not match type integer");
}

TEST(ConfigValue, positiveNumConstraint)
{
    auto positiveNum = ConfigValue{ConfigType::Integer}.defaultValue(20u).withConstraint(ValidateUint16);
    auto const err = positiveNum.setValue(-22, "key");
    EXPECT_TRUE(err.has_value());
    EXPECT_EQ(err->error, "key number does not satisfy the specified constraint");
    EXPECT_FALSE(positiveNum.setValue(99, "key"));

    auto doubleVal = ConfigValue{ConfigType::Double}.defaultValue(0.2).withConstraint(ValidatePositiveDouble);
    auto const err2 = doubleVal.setValue(-1.1);
    EXPECT_TRUE(err2.has_value());
    EXPECT_EQ(err2->error, "double number must be greater than 0");
    EXPECT_FALSE(doubleVal.setValue(12.1));
}

void
testConstraintDeath(Constraint const& constraint)
{
    EXPECT_DEATH(
        { [[maybe_unused]] auto a = ConfigValue{ConfigType::Boolean}.defaultValue(true).withConstraint(constraint); },
        ".*"
    );
}

TEST(ConfigValueDeathTest, withConstraintWrongType)
{
    // test_p doesn't work with Constraint* as type
    testConstraintDeath(validatePort);
    testConstraintDeath(validateChannelName);
    testConstraintDeath(validateLogLevelName);
    testConstraintDeath(validateIP);
    testConstraintDeath(validateCassandraName);
    testConstraintDeath(validateLoadMode);
    testConstraintDeath(validateLogTag);
    testConstraintDeath(validateApiVersion);
    testConstraintDeath(ValidateUint16);
    testConstraintDeath(ValidateUint32);
    testConstraintDeath(ValidateUint64);
    testConstraintDeath(ValidatePositiveDouble);
}

TEST(ConfigValueDeathTest, withConstraintWrongValues)
{
    EXPECT_DEATH(
        {
            [[maybe_unused]] auto a = ConfigValue{ConfigType::String}.defaultValue(33).withConstraint(validateLoadMode);
        },
        ".*"
    );
    EXPECT_DEATH(
        {
            [[maybe_unused]] auto a =
                ConfigValue{ConfigType::Integer}.defaultValue(999999).withConstraint(ValidateUint16);
        },
        ".*"
    );
    EXPECT_DEATH(
        {
            [[maybe_unused]] auto a = ConfigValue{ConfigType::Integer}.defaultValue(-66).withConstraint(ValidateUint64);
        },
        ".*"
    );
    EXPECT_DEATH({ [[maybe_unused]] auto a = ConfigValue{ConfigType::Boolean}.defaultValue(-66); }, ".*");
}
