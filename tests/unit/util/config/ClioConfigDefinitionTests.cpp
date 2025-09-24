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

#include "util/MockAssert.hpp"
#include "util/config/Array.hpp"
#include "util/config/ArrayView.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigDescription.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/FakeConfigData.hpp"
#include "util/config/Types.hpp"
#include "util/config/ValueView.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

using namespace util::config;

struct ConfigTest : virtual testing::Test {
    ClioConfigDefinition const configData = generateConfig();
};

TEST_F(ConfigTest, fetchValues)
{
    auto const v = configData.getValueView("header.port");
    EXPECT_EQ(v.type(), ConfigType::Integer);

    EXPECT_EQ("value", configData.getValueView("header.text1").asString());
    EXPECT_EQ(123, configData.getValueView("header.port").asIntType<int>());
    EXPECT_EQ(true, configData.getValueView("header.admin").asBool());
    EXPECT_EQ("TSM", configData.getValueView("header.sub.sub2Value").asString());
    EXPECT_EQ(444.22, configData.getValueView("ip").asDouble());
}

TEST_F(ConfigTest, fetchValuesByTemplate)
{
    EXPECT_EQ("value", configData.get<std::string>("header.text1"));
    EXPECT_EQ(123, configData.get<int>("header.port"));
    EXPECT_EQ(true, configData.get<bool>("header.admin"));
    EXPECT_EQ("TSM", configData.get<std::string>("header.sub.sub2Value"));
    EXPECT_EQ(444.22, configData.get<double>("ip"));
}

TEST_F(ConfigTest, fetchOptionalValues)
{
    EXPECT_EQ(std::nullopt, configData.maybeValue<double>("optional.withNoDefault"));
    EXPECT_EQ(0.0, configData.maybeValue<double>("optional.withDefault"));
}

TEST_F(ConfigTest, fetchObjectDirectly)
{
    auto const obj = configData.getObject("header");
    EXPECT_TRUE(obj.containsKey("sub.sub2Value"));

    auto const obj2 = obj.getObject("sub");
    EXPECT_TRUE(obj2.containsKey("sub2Value"));
    EXPECT_EQ(obj2.getValueView("sub2Value").asString(), "TSM");
}

TEST_F(ConfigTest, CheckKeys)
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

TEST_F(ConfigTest, CheckAllKeys)
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

struct ConfigAssertTest : common::util::WithMockAssert, ConfigTest {};

TEST_F(ConfigAssertTest, GetNonExistentKeys)
{
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = configData.getValueView("head."); });
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = configData.getValueView("asdf"); });
}

TEST_F(ConfigAssertTest, GetValueButIsArray)
{
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = configData.getValueView("dosguard.whitelist"); });
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = configData.getValueView("dosguard.whitelist.[]"); });
}

TEST_F(ConfigAssertTest, GetNonExistentObjectKey)
{
    ASSERT_FALSE(configData.contains("head"));
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = configData.getObject("head"); });
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = configData.getObject("doesNotExist"); });
}

TEST_F(ConfigAssertTest, GetObjectButIsArray)
{
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = configData.getObject("array"); });
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = configData.getObject("array", 2); });
}

TEST_F(ConfigAssertTest, GetArrayButIsValue)
{
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = configData.getArray("header.text1"); });
}

TEST_F(ConfigAssertTest, GetNonExistentArrayKey)
{
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto unused = configData.getArray("asdf"); });
}

TEST(ConfigDescription, GetValues)
{
    ClioConfigDescription const definition{};

    EXPECT_EQ(
        definition.get("database.type"),
        "Specifies the type of database used for storing and retrieving data required by the Clio server. Both "
        "ScyllaDB and Cassandra can serve as backends for Clio; however, this value must be set to `cassandra`."
    );
    EXPECT_EQ(definition.get("etl_sources.[].ip"), "The IP address of the ETL source.");
    EXPECT_EQ(definition.get("prometheus.enabled"), "Enables or disables Prometheus metrics.");
}

struct ConfigDescriptionAssertTest : common::util::WithMockAssert {};

TEST_F(ConfigDescriptionAssertTest, NonExistingKeyTest)
{
    ClioConfigDescription const definition{};

    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto a = definition.get("data"); });
    EXPECT_CLIO_ASSERT_FAIL({ [[maybe_unused]] auto a = definition.get("etl_sources.[]"); });
}

/** @brief Testing override the default values with the ones in Json */
struct OverrideConfigVals : testing::Test {
    OverrideConfigVals()
    {
        ConfigFileJson const jsonFileObj{boost::json::parse(kJSON_DATA).as_object()};
        auto const errors = configData.parse(jsonFileObj);
        EXPECT_TRUE(!errors.has_value());
    }
    ClioConfigDefinition configData = generateConfig();
};

TEST_F(OverrideConfigVals, ValidateValuesStrings)
{
    // make sure the values in configData are overridden
    EXPECT_TRUE(configData.contains("header.text1"));
    EXPECT_EQ(configData.getValueView("header.text1").asString(), "value");

    EXPECT_FALSE(configData.contains("header.sub"));
    EXPECT_TRUE(configData.contains("header.sub.sub2Value"));
    EXPECT_EQ(configData.getValueView("header.sub.sub2Value").asString(), "TSM");

    EXPECT_TRUE(configData.contains("requireValue"));
    EXPECT_EQ(configData.getValueView("requireValue").asString(), "required");
}

TEST_F(OverrideConfigVals, ValidateValuesDouble)
{
    EXPECT_TRUE(configData.contains("optional.withDefault"));
    EXPECT_EQ(configData.getValueView("optional.withDefault").asDouble(), 0.0);

    // make sure the values not overwritten, (default values) are there too
    EXPECT_TRUE(configData.contains("ip"));
    EXPECT_EQ(configData.getValueView("ip").asDouble(), 444.22);
}

TEST_F(OverrideConfigVals, ValidateValuesInteger)
{
    EXPECT_TRUE(configData.contains("dosguard.port"));
    EXPECT_EQ(configData.getValueView("dosguard.port").asIntType<int>(), 44444);

    EXPECT_TRUE(configData.contains("header.port"));
    EXPECT_EQ(configData.getValueView("header.port").asIntType<int64_t>(), 321);
}

TEST_F(OverrideConfigVals, ValidateValuesBool)
{
    EXPECT_TRUE(configData.contains("header.admin"));
    EXPECT_EQ(configData.getValueView("header.admin").asBool(), false);
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

    EXPECT_EQ(objInArr.getValueView("sub").asDouble(), 111.11);
    EXPECT_EQ(objInArr.getValueView("sub2").asString(), "subCategory");
    EXPECT_EQ(obj2InArr.getValueView("sub").asDouble(), 4321.55);
    EXPECT_EQ(obj2InArr.getValueView("sub2").asString(), "temporary");
    EXPECT_EQ(obj3InArr.getValueView("sub").asDouble(), 5555.44);
    EXPECT_EQ(obj3InArr.getValueView("sub2").asString(), "london");
}

struct IncorrectOverrideValues : testing::Test {
    ClioConfigDefinition configData = generateConfig();
};

TEST_F(IncorrectOverrideValues, InvalidJsonErrors)
{
    ConfigFileJson const jsonFileObj{boost::json::parse(kINVALID_JSON_DATA).as_object()};
    auto const errors = configData.parse(jsonFileObj);
    EXPECT_TRUE(errors.has_value());

    // Expected error messages
    std::set<std::string_view> const expectedErrors{
        "dosguard.whitelist.[] value does not match type string",
        "header.port value does not match type integer",
        "header.admin value does not match type boolean",
        "optional.withDefault value does not match type double"
    };

    std::set<std::string_view> actualErrors;
    for (auto const& error : errors.value()) {
        actualErrors.insert(error.error);
    }
    EXPECT_EQ(expectedErrors, actualErrors);
}

struct ClioConfigDefinitionParseArrayTest : public virtual ::testing::Test {
    ClioConfigDefinition config{
        {"array.[].int", Array{ConfigValue{ConfigType::Integer}}},
        {"array.[].string", Array{ConfigValue{ConfigType::String}.optional()}}
    };
};

TEST_F(ClioConfigDefinitionParseArrayTest, emptyArray)
{
    auto const configJson = boost::json::parse(R"JSON({
        "array": []
    })JSON")
                                .as_object();

    auto const result = config.parse(ConfigFileJson{configJson});
    EXPECT_FALSE(result.has_value());
}

TEST_F(ClioConfigDefinitionParseArrayTest, emptyJson)
{
    auto const configJson = boost::json::object{};

    auto const result = config.parse(ConfigFileJson{configJson});
    EXPECT_FALSE(result.has_value());
}

TEST_F(ClioConfigDefinitionParseArrayTest, fullArray)
{
    auto const configJson = boost::json::parse(R"JSON({
        "array": [
            {"int": 1, "string": "one"},
            {"int": 2, "string": "two"}
        ]
    })JSON")
                                .as_object();

    auto const result = config.parse(ConfigFileJson{configJson});
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(config.arraySize("array.[]"), 2);
}

TEST_F(ClioConfigDefinitionParseArrayTest, onlyRequiredFields)
{
    auto const configJson = boost::json::parse(R"JSON({
        "array": [
            {"int": 1},
            {"int": 2}
        ]
    })JSON")
                                .as_object();

    auto const configFile = ConfigFileJson{configJson};
    auto const result = config.parse(configFile);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(config.arraySize("array.[]"), 2);

    EXPECT_EQ(config.getArray("array.[].int").valueAt(0).asIntType<int>(), 1);
    EXPECT_EQ(config.getArray("array.[].int").valueAt(1).asIntType<int>(), 2);
    EXPECT_FALSE(config.getArray("array.[].string").valueAt(0).hasValue());
    EXPECT_FALSE(config.getArray("array.[].string").valueAt(1).hasValue());
}

TEST_F(ClioConfigDefinitionParseArrayTest, someOptionalFieldsMissing)
{
    auto const configJson = boost::json::parse(R"JSON({
        "array": [
            {"int": 1, "string": "one"},
            {"int": 2}
        ]
    })JSON")
                                .as_object();

    auto const configFile = ConfigFileJson{configJson};
    auto const result = config.parse(configFile);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(config.arraySize("array.[]"), 2);

    EXPECT_EQ(config.getArray("array.[].int").valueAt(0).asIntType<int>(), 1);
    EXPECT_EQ(config.getArray("array.[].int").valueAt(1).asIntType<int>(), 2);
    EXPECT_EQ(config.getArray("array.[].string").valueAt(0).asString(), "one");
    EXPECT_FALSE(config.getArray("array.[].string").valueAt(1).hasValue());
}

TEST_F(ClioConfigDefinitionParseArrayTest, optionalFieldMissingAtFirstPosition)
{
    auto const configJson = boost::json::parse(R"JSON({
        "array": [
            {"int": 1},
            {"int": 2, "string": "two"}
        ]
    })JSON")
                                .as_object();

    auto const configFile = ConfigFileJson{configJson};
    auto const result = config.parse(configFile);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(config.arraySize("array.[]"), 2);

    EXPECT_EQ(config.getArray("array.[].int").valueAt(0).asIntType<int>(), 1);
    EXPECT_EQ(config.getArray("array.[].int").valueAt(1).asIntType<int>(), 2);

    EXPECT_FALSE(config.getArray("array.[].string").valueAt(0).hasValue());
    EXPECT_EQ(config.getArray("array.[].string").valueAt(1).asString(), "two");
}

TEST_F(ClioConfigDefinitionParseArrayTest, missingRequiredFields)
{
    auto const configJson = boost::json::parse(R"JSON({
        "array": [
            {"int": 1},
            {"string": "two"}
        ]
    })JSON")
                                .as_object();

    auto const configFile = ConfigFileJson{configJson};
    auto const result = config.parse(configFile);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
    EXPECT_THAT(result->at(0).error, testing::StartsWith("The value of array.[].int"));
}

TEST_F(ClioConfigDefinitionParseArrayTest, missingAllRequiredFields)
{
    auto const configJson = boost::json::parse(R"JSON({
        "array": [
            {"string": "one"},
            {"string": "two"}
        ]
    })JSON")
                                .as_object();

    auto const configFile = ConfigFileJson{configJson};
    auto const result = config.parse(configFile);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1);
    EXPECT_THAT(result->at(0).error, testing::StartsWith("The value of array.[].int"));
}

TEST(ClioConfigDefinitionParse, unexpectedFields)
{
    ClioConfigDefinition config{
        {"expected", ConfigValue{ConfigType::String}.optional()},
    };

    auto const configJson = boost::json::parse(R"JSON({
        "expected": "present",
        "unexpected_string": "",
        "unexpected_non_empty_array": [
            {"string": ""},
            {"string": ""}
        ],
        "unexpected_empty_array": [],
        "unexpected_object": {
            "string": ""
        }
    })JSON")
                                .as_object();

    auto const configFile = ConfigFileJson{configJson};
    auto result = config.parse(configFile);
    std::ranges::sort(*result, [](auto const& lhs, auto const& rhs) { return lhs.error < rhs.error; });
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->size(), 4);

    EXPECT_EQ(result->at(0).error, "Unknown key: unexpected_empty_array.[]");
    EXPECT_EQ(result->at(1).error, "Unknown key: unexpected_non_empty_array.[].string");
    EXPECT_EQ(result->at(2).error, "Unknown key: unexpected_object.string");
    EXPECT_EQ(result->at(3).error, "Unknown key: unexpected_string");
}
