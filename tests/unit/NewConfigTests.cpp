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

#include <string_view>
#include <unordered_set>

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

    auto const v2 = configData.getValueInArray("dosguard.whitelist", 0);
    EXPECT_EQ(v2.asString(), "125.5.5.2");
}

TEST_F(NewConfigTest, fetchObject)
{
    auto const obj = configData.getObject("header");
    EXPECT_TRUE(obj.containsKey("sub.sub2Value"));

    auto const obj2 = obj.getObject("sub");
    EXPECT_TRUE(obj2.containsKey("sub2Value"));
    EXPECT_EQ(obj2.getValue("sub2Value").asString(), "TSM");

    auto const objInArr = configData.getObject("array", 0);
    auto const obj2InArr = configData.getObject("array", 1);
    EXPECT_EQ(objInArr.getValue("sub").asDouble(), 111.11);
    EXPECT_EQ(objInArr.getValue("sub2").asString(), "subCategory");
    EXPECT_EQ(obj2InArr.getValue("sub").asDouble(), 4321.55);
    EXPECT_EQ(obj2InArr.getValue("sub2").asString(), "temporary");
}

TEST_F(NewConfigTest, fetchArray)
{
    auto const obj = configData.getObject("dosguard");
    EXPECT_TRUE(obj.containsKey("whitelist.[]"));

    auto const arr = obj.getArray("whitelist");
    EXPECT_EQ(2, arr.size());

    auto const sameArr = configData.getArray("dosguard.whitelist");
    EXPECT_EQ(2, sameArr.size());
    EXPECT_EQ(sameArr.valueAt(0).asString(), arr.valueAt(0).asString());
    EXPECT_EQ(sameArr.valueAt(1).asString(), arr.valueAt(1).asString());
}

TEST_F(NewConfigTest, CheckKeys)
{
    EXPECT_TRUE(configData.contains("header.port"));
    EXPECT_TRUE(configData.contains("array.[].sub"));
    EXPECT_TRUE(configData.contains("dosguard.whitelist.[]"));
    EXPECT_FALSE(configData.contains("dosguard.whitelist"));

    EXPECT_TRUE(configData.startsWith("dosguard"));
    EXPECT_TRUE(configData.startsWith("ip"));

    EXPECT_EQ(configData.arraySize("array"), 2);
    EXPECT_EQ(configData.arraySize("higher"), 1);
    EXPECT_EQ(configData.arraySize("dosguard.whitelist"), 2);
}

TEST_F(NewConfigTest, CheckAllKeys)
{
    auto expected = std::unordered_set<std::string_view>{};
    auto const actual = std::unordered_set<std::string_view>{
        "header.text1",
        "header.port",
        "header.admin",
        "header.sub.sub2Value",
        "ip",
        "array.[].sub",
        "array.[].sub2",
        "higher.[].low.section",
        "higher.[].low.admin",
        "dosguard.whitelist.[]",
        "dosguard.port"
    };

    for (auto i = configData.begin(); i != configData.end(); ++i) {
        expected.emplace((i->first));
    }
    EXPECT_EQ(expected, actual);
}

struct NewConfigDeathTest : NewConfigTest {};

TEST_F(NewConfigDeathTest, IncorrectGetValues)
{
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getValue("head"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getValue("head."); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getValue("asdf"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getValue("dosguard.whitelist"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getValue("dosguard.whitelist.[]"); }, ".*");
}

TEST_F(NewConfigDeathTest, IncorrectGetObject)
{
    ASSERT_FALSE(configData.contains("head"));
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getObject("head"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getObject("array"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getObject("array", 2); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getObject("doesNotExist"); }, ".*");
}

TEST_F(NewConfigDeathTest, IncorrectGetArray)
{
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getArray("header.text1"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getArray("asdf"); }, ".*");
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
