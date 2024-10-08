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

#include "util/TmpFile.hpp"
#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigDescription.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/FakeConfigData.hpp"
#include "util/newconfig/Types.hpp"
#include "util/newconfig/ValueView.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

using namespace util::config;

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

TEST_F(NewConfigTest, fetchObjectDirectly)
{
    auto const obj = configData.getObject("header");
    EXPECT_TRUE(obj.containsKey("sub.sub2Value"));

    auto const obj2 = obj.getObject("sub");
    EXPECT_TRUE(obj2.containsKey("sub2Value"));
    EXPECT_EQ(obj2.getValue("sub2Value").asString(), "TSM");
}

TEST_F(NewConfigTest, CheckKeys)
{
    EXPECT_TRUE(configData.contains("header.port"));
    EXPECT_TRUE(configData.contains("array.[].sub"));
    EXPECT_TRUE(configData.contains("dosguard.whitelist.[]"));
    EXPECT_FALSE(configData.contains("dosguard.whitelist"));

    EXPECT_TRUE(configData.hasItemsWithPrefix("dosguard"));
    EXPECT_TRUE(configData.hasItemsWithPrefix("ip"));

    // all arrays currently not populated, only has "itemPattern_" that defines
    // the type/constraint each configValue will have later on
    EXPECT_EQ(configData.arraySize("array"), 0);
    EXPECT_EQ(configData.arraySize("higher"), 0);
    EXPECT_EQ(configData.arraySize("dosguard.whitelist"), 0);
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
        "dosguard.port",
        "optional.withDefault",
        "optional.withNoDefault",
        "requireValue"
    };

    for (auto i = configData.begin(); i != configData.end(); ++i) {
        expected.emplace((i->first));
    }
    EXPECT_EQ(expected, actual);
}

struct NewConfigDeathTest : NewConfigTest {};

TEST_F(NewConfigDeathTest, GetNonExistentKeys)
{
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getValue("head."); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getValue("asdf"); }, ".*");
}

TEST_F(NewConfigDeathTest, GetValueButIsArray)
{
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getValue("dosguard.whitelist"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getValue("dosguard.whitelist.[]"); }, ".*");
}

TEST_F(NewConfigDeathTest, GetNonExistentObjectKey)
{
    ASSERT_FALSE(configData.contains("head"));
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getObject("head"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getObject("doesNotExist"); }, ".*");
}

TEST_F(NewConfigDeathTest, GetObjectButIsArray)
{
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getObject("array"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getObject("array", 2); }, ".*");
}

TEST_F(NewConfigDeathTest, GetArrayButIsValue)
{
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getArray("header.text1"); }, ".*");
}

TEST_F(NewConfigDeathTest, GetNonExistentArrayKey)
{
    EXPECT_DEATH({ [[maybe_unused]] auto a_ = configData.getArray("asdf"); }, ".*");
}

TEST(ConfigDescription, GetValues)
{
    ClioConfigDescription const definition{};

    EXPECT_EQ(definition.get("database.type"), "Type of database to use.");
    EXPECT_EQ(definition.get("etl_sources.[].ip"), "IP address of the ETL source.");
    EXPECT_EQ(definition.get("prometheus.enabled"), "Enable or disable Prometheus metrics.");
}

TEST(ConfigDescriptionAssertDeathTest, NonExistingKeyTest)
{
    ClioConfigDescription const definition{};

    EXPECT_DEATH({ [[maybe_unused]] auto a = definition.get("data"); }, ".*");
    EXPECT_DEATH({ [[maybe_unused]] auto a = definition.get("etl_sources.[]"); }, ".*");
}

/** @brief Testing override the default values with the ones in Json */
struct OverrideConfigVals : testing::Test {
    OverrideConfigVals()
    {
        ConfigFileJson const jsonFileObj{boost::json::parse(JSONData).as_object()};
        auto const errors = configData.parse(jsonFileObj);
        EXPECT_TRUE(!errors.has_value());
    }
    ClioConfigDefinition configData = generateConfig();
};

TEST_F(OverrideConfigVals, ValidateValuesStrings)
{
    // make sure the values in configData are overriden
    EXPECT_TRUE(configData.contains("header.text1"));
    EXPECT_EQ(configData.getValue("header.text1").asString(), "value");

    EXPECT_FALSE(configData.contains("header.sub"));
    EXPECT_TRUE(configData.contains("header.sub.sub2Value"));
    EXPECT_EQ(configData.getValue("header.sub.sub2Value").asString(), "TSM");

    EXPECT_TRUE(configData.contains("requireValue"));
    EXPECT_EQ(configData.getValue("requireValue").asString(), "required");
}

TEST_F(OverrideConfigVals, ValidateValuesDouble)
{
    EXPECT_TRUE(configData.contains("optional.withDefault"));
    EXPECT_EQ(configData.getValue("optional.withDefault").asDouble(), 0.0);

    // make sure the values not overwritten, (default values) are there too
    EXPECT_TRUE(configData.contains("ip"));
    EXPECT_EQ(configData.getValue("ip").asDouble(), 444.22);
}

TEST_F(OverrideConfigVals, ValidateValuesInteger)
{
    EXPECT_TRUE(configData.contains("dosguard.port"));
    EXPECT_EQ(configData.getValue("dosguard.port").asIntType<int>(), 44444);

    EXPECT_TRUE(configData.contains("header.port"));
    EXPECT_EQ(configData.getValue("header.port").asIntType<int64_t>(), 321);
}

TEST_F(OverrideConfigVals, ValidateValuesBool)
{
    EXPECT_TRUE(configData.contains("header.admin"));
    EXPECT_EQ(configData.getValue("header.admin").asBool(), false);
}

TEST_F(OverrideConfigVals, ValidateIntegerValuesInArrays)
{
    // Check array values (sub)
    EXPECT_TRUE(configData.contains("array.[].sub"));
    auto const arrSub = configData.getArray("array.[].sub");

    std::vector<double> expectedArrSubVal{111.11, 4321.55, 5555.44};
    std::vector<double> actualArrSubVal{};
    for (auto it = arrSub.begin<ValueView>(); it != arrSub.end<ValueView>(); ++it) {
        actualArrSubVal.emplace_back((*it).asDouble());
    }
    EXPECT_TRUE(std::ranges::equal(expectedArrSubVal, actualArrSubVal));
}

TEST_F(OverrideConfigVals, ValidateStringValuesInArrays)
{
    // Check array values (sub2)
    EXPECT_TRUE(configData.contains("array.[].sub2"));
    auto const arrSub2 = configData.getArray("array.[].sub2");

    std::vector<std::string> expectedArrSub2Val{"subCategory", "temporary", "london"};
    std::vector<std::string> actualArrSub2Val{};
    for (auto it = arrSub2.begin<ValueView>(); it != arrSub2.end<ValueView>(); ++it) {
        actualArrSub2Val.emplace_back((*it).asString());
    }
    EXPECT_TRUE(std::ranges::equal(expectedArrSub2Val, actualArrSub2Val));

    // Check dosguard values
    EXPECT_TRUE(configData.contains("dosguard.whitelist.[]"));
    auto const dosguard = configData.getArray("dosguard.whitelist.[]");
    EXPECT_EQ("125.5.5.1", dosguard.valueAt(0).asString());
    EXPECT_EQ("204.2.2.1", dosguard.valueAt(1).asString());
}

TEST_F(OverrideConfigVals, FetchArray)
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

TEST_F(OverrideConfigVals, FetchObjectByArray)
{
    auto const objInArr = configData.getObject("array", 0);
    auto const obj2InArr = configData.getObject("array", 1);
    auto const obj3InArr = configData.getObject("array", 2);

    EXPECT_EQ(objInArr.getValue("sub").asDouble(), 111.11);
    EXPECT_EQ(objInArr.getValue("sub2").asString(), "subCategory");
    EXPECT_EQ(obj2InArr.getValue("sub").asDouble(), 4321.55);
    EXPECT_EQ(obj2InArr.getValue("sub2").asString(), "temporary");
    EXPECT_EQ(obj3InArr.getValue("sub").asDouble(), 5555.44);
    EXPECT_EQ(obj3InArr.getValue("sub2").asString(), "london");
}

struct IncorrectOverrideValues : testing::Test {
    ClioConfigDefinition configData = generateConfig();
};

TEST_F(IncorrectOverrideValues, InvalidJsonErrors)
{
    ConfigFileJson const jsonFileObj{boost::json::parse(invalidJSONData).as_object()};
    auto const errors = configData.parse(jsonFileObj);
    EXPECT_TRUE(errors.has_value());

    // Expected error messages
    std::unordered_set<std::string_view> const expectedErrors{
        "dosguard.whitelist.[] value does not match type string",
        "higher.[].low.section key is required in user Config",
        "higher.[].low.admin key is required in user Config",
        "array.[].sub key is required in user Config",
        "header.port value does not match type integer",
        "header.admin value does not match type boolean",
        "optional.withDefault value does not match type double"
    };

    std::unordered_set<std::string_view> actualErrors;
    for (auto const& error : errors.value()) {
        actualErrors.insert(error.error);
    }
    EXPECT_EQ(expectedErrors, actualErrors);
}
