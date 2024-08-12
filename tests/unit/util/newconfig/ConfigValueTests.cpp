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

TEST(ConfigValue, withDifferentConstraints)
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

    auto cvChannel = ConfigValue{ConfigType::String}.defaultValue("General").withConstraint(validateChannelName);
    auto const err2 = cvChannel.setValue("nono");
    EXPECT_TRUE(err2.has_value());
    EXPECT_EQ(
        err2->error, "Channel name must be one of General, WebServer, Backend, RPC, ETL, Subscriptions, Performance"
    );
    EXPECT_FALSE(cvChannel.setValue("Performance"));

    auto logLevelChannel = ConfigValue{ConfigType::String}.defaultValue("info").withConstraint(validateLogLevelName);
    auto const err3 = logLevelChannel.setValue("PETER_WAS_HERE");
    EXPECT_TRUE(err3.has_value());
    EXPECT_EQ(err3->error, "log_level must be one of trace, debug, info, warning, error, fatal, count");
    EXPECT_FALSE(logLevelChannel.setValue("count"));

    auto ip = ConfigValue{ConfigType::String}.defaultValue("127.0.0.1").withConstraint(validateIP);
    auto const err4 = ip.setValue("123.44");
    EXPECT_TRUE(err4.has_value());
    EXPECT_EQ(err4->error, "ip is not a valid ip address");
    EXPECT_FALSE(ip.setValue("126.0.0.2"));

    auto cassandra = ConfigValue{ConfigType::String}.defaultValue("cassandra").withConstraint(validateCassandraName);
    auto const casError = cassandra.setValue("123.44");
    EXPECT_TRUE(casError.has_value());
    EXPECT_EQ(casError->error, "database.type must be string Cassandra");
    EXPECT_FALSE(cassandra.setValue("cassandra"));

    auto load = ConfigValue{ConfigType::String}.defaultValue("async").withConstraint(validateLoadMode);
    auto const err5 = load.setValue("ASYCS");
    EXPECT_TRUE(err5.has_value());
    EXPECT_EQ(err5->error, "cache.load must be string sync, async, or none");
    EXPECT_FALSE(load.setValue("none"));
    EXPECT_FALSE(load.setValue("sync"));

    auto logTagName = ConfigValue{ConfigType::String}.defaultValue("uint").withConstraint(validateLogTag);
    auto const err6 = logTagName.setValue("idek_anymore");
    EXPECT_TRUE(err6.has_value());
    EXPECT_EQ(err6->error, "log_tag_style must be string int, uint, null, none, or uuid");
    EXPECT_FALSE(logTagName.setValue("null"));
    EXPECT_FALSE(logTagName.setValue("uuid"));

    auto apiVer = ConfigValue{ConfigType::Integer}.defaultValue(1u).withConstraint(validateApiVersion);
    auto const err7 = apiVer.setValue(9999);
    EXPECT_TRUE(err7.has_value());
    EXPECT_EQ(err7->error, fmt::format("api_version must be between {} and {}", API_VERSION_MIN, API_VERSION_MAX));
    EXPECT_FALSE(apiVer.setValue(API_VERSION_MAX));
    auto const apiWrongType = apiVer.setValue("9");
    EXPECT_TRUE(apiWrongType.has_value());
    EXPECT_EQ(apiWrongType->error, "value does not match type integer");
}

TEST(ConfigValueDeathTest, withDifferentConstraints)
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
                ConfigValue{ConfigType::Boolean}.defaultValue(true).withConstraint(validateLogTag);
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
}
