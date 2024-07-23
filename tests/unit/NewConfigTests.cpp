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
#include "util/newconfig/ConfigDescription.hpp"
#include "util/newconfig/ConfigValue.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <gtest/gtest.h>
#include <newconfig/FakeConfigData.hpp>

using namespace util::config;

// TODO: parsing config file and populating into config will be here once implemented
struct NewConfigTest : testing::Test {
    ClioConfigDefinition const configData = generateConfig();
};

TEST_F(NewConfigTest, fetchValues)
{
    auto const v = configData.getValue("header.port");
    EXPECT_EQ(v.type(), ConfigType::Integer);

    EXPECT_EQ("value", configData.getValue("header.text1").asString());
    EXPECT_EQ(123, configData.getValue("header.port").asIntType<int>());
    EXPECT_EQ(true, configData.getValue("header.admin").asBool());
    EXPECT_EQ("TSM", configData.getValue("header.sub.sub2Value").asString());
    EXPECT_EQ(444.22, configData.getValue("ip").asDouble());
}

TEST_F(NewConfigTest, fetchObject)
{
    auto const obj = configData.getObject("header");
    ASSERT_TRUE(obj.containsKey("sub.sub2Value"));

    auto const obj2 = obj.getObject("sub");
    ASSERT_TRUE(obj2.containsKey("sub2Value"));
    EXPECT_EQ(obj2.getValue("sub2Value").asString(), "TSM");
}

TEST_F(NewConfigTest, fetchArray)
{
    auto const obj = configData.getObject("dosguard");
    ASSERT_TRUE(obj.containsKey("whitelist.[]"));

    auto arr = obj.getArray("whitelist");
    EXPECT_EQ(2, arr.size());
}

TEST(ConfigDescription, getValues)
{
    ClioConfigDescription definition{};

    EXPECT_EQ(definition.get("database.type"), "Type of database to use.");
    EXPECT_EQ(definition.get("etl_source.[].ip"), "IP address of the ETL source.");
    EXPECT_EQ(definition.get("prometheus.enabled"), "Enable or disable Prometheus metrics.");
}

TEST(ConfigDescriptionAssertDeathTest, nonExistingKeyTest)
{
    ClioConfigDescription definition{};

    EXPECT_DEATH(definition.get("data"), ".*");
    EXPECT_DEATH(definition.get("etl_source.[]"), ".*");
}
