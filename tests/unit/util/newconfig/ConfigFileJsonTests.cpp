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
#include "util/NameGenerator.hpp"
#include "util/OverloadSet.hpp"
#include "util/TmpFile.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/FakeConfigData.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct ConfigFileJsonOldTest : NoLoggerFixture {};

TEST_F(ConfigFileJsonOldTest, createUsingCorrectFile)
{
    auto const jsonFileObj = ConfigFileJson::makeConfigFileJson(TmpFile(kJSON_DATA).path);
    EXPECT_TRUE(jsonFileObj.has_value());

    EXPECT_TRUE(jsonFileObj->containsKey("array.[].sub"));
    auto const arrSub = jsonFileObj->getArray("array.[].sub");
    EXPECT_EQ(arrSub.size(), 3);
}

TEST_F(ConfigFileJsonOldTest, createUsingIncorrectFileReturnsError)
{
    auto const jsonFileObj = util::config::ConfigFileJson::makeConfigFileJson("123/clio");
    EXPECT_FALSE(jsonFileObj.has_value());
}

struct ConfigFileJsonParseOldTest : ConfigFileJsonOldTest {
    ConfigFileJsonParseOldTest() : jsonFileObj{boost::json::parse(kJSON_DATA).as_object()}
    {
    }

    ConfigFileJson const jsonFileObj;
};

TEST_F(ConfigFileJsonParseOldTest, validateValues)
{
    EXPECT_TRUE(jsonFileObj.containsKey("header.text1"));
    EXPECT_EQ(std::get<std::string>(jsonFileObj.getValue("header.text1")), "value");

    EXPECT_TRUE(jsonFileObj.containsKey("header.sub.sub2Value"));
    EXPECT_EQ(std::get<std::string>(jsonFileObj.getValue("header.sub.sub2Value")), "TSM");

    EXPECT_TRUE(jsonFileObj.containsKey("dosguard.port"));
    EXPECT_EQ(std::get<int64_t>(jsonFileObj.getValue("dosguard.port")), 44444);

    EXPECT_FALSE(jsonFileObj.containsKey("idk"));
    EXPECT_FALSE(jsonFileObj.containsKey("optional.withNoDefault"));
}

TEST_F(ConfigFileJsonParseOldTest, validateArrayValue)
{
    // validate array.[].sub matches expected values
    EXPECT_TRUE(jsonFileObj.containsKey("array.[].sub"));
    auto const arrSub = jsonFileObj.getArray("array.[].sub");
    EXPECT_EQ(arrSub.size(), 3);

    std::vector<double> expectedArrSubVal{111.11, 4321.55, 5555.44};
    std::vector<double> actualArrSubVal{};

    for (auto it = arrSub.begin(); it != arrSub.end(); ++it) {
        ASSERT_TRUE(std::holds_alternative<double>(*it));
        actualArrSubVal.emplace_back(std::get<double>(*it));
    }
    EXPECT_TRUE(std::ranges::equal(expectedArrSubVal, actualArrSubVal));

    // validate array.[].sub2 matches expected values
    EXPECT_TRUE(jsonFileObj.containsKey("array.[].sub2"));
    auto const arrSub2 = jsonFileObj.getArray("array.[].sub2");
    EXPECT_EQ(arrSub2.size(), 3);
    std::vector<std::string> expectedArrSub2Val{"subCategory", "temporary", "london"};
    std::vector<std::string> actualArrSub2Val{};

    for (auto it = arrSub2.begin(); it != arrSub2.end(); ++it) {
        ASSERT_TRUE(std::holds_alternative<std::string>(*it));
        actualArrSub2Val.emplace_back(std::get<std::string>(*it));
    }
    EXPECT_TRUE(std::ranges::equal(expectedArrSub2Val, actualArrSub2Val));

    EXPECT_TRUE(jsonFileObj.containsKey("dosguard.whitelist.[]"));
    auto const whitelistArr = jsonFileObj.getArray("dosguard.whitelist.[]");
    EXPECT_EQ(whitelistArr.size(), 2);
    EXPECT_EQ("125.5.5.1", std::get<std::string>(whitelistArr.at(0)));
    EXPECT_EQ("204.2.2.1", std::get<std::string>(whitelistArr.at(1)));
}

struct ConfigValueJsonGetArrayDeathTest : ConfigFileJsonParseOldTest {};

TEST_F(ConfigValueJsonGetArrayDeathTest, invalidGetArray)
{
    EXPECT_DEATH([[maybe_unused]] auto a = jsonFileObj.getArray("header.text1"), ".*");
}

struct JsonFromTempFile : testing::Test {
    JsonFromTempFile() : jsonFileObj{util::config::ConfigFileJson::makeConfigFileJson(TmpFile(kJSON_DATA).path).value()}
    {
    }

    ConfigFileJson jsonFileObj;
};

TEST_F(JsonFromTempFile, validateKeys)
{
    EXPECT_TRUE(jsonFileObj.containsKey("header.text1"));
    EXPECT_TRUE(jsonFileObj.containsKey("header.sub.sub2Value"));
    EXPECT_TRUE(jsonFileObj.containsKey("dosguard.port"));
    EXPECT_FALSE(jsonFileObj.containsKey("idk"));
    EXPECT_FALSE(jsonFileObj.containsKey("optional.withNoDefault"));
}

TEST_F(JsonFromTempFile, validateValues)
{
    EXPECT_EQ(std::get<std::string>(jsonFileObj.getValue("header.text1")), "value");
    EXPECT_EQ(std::get<std::string>(jsonFileObj.getValue("header.sub.sub2Value")), "TSM");
    EXPECT_EQ(std::get<int64_t>(jsonFileObj.getValue("dosguard.port")), 44444);
}

TEST_F(JsonFromTempFile, validateArrayValue)
{
    // validate array.[].sub matches expected values
    EXPECT_TRUE(jsonFileObj.containsKey("array.[].sub"));
    auto const arrSub = jsonFileObj.getArray("array.[].sub");
    EXPECT_EQ(arrSub.size(), 3);

    std::vector<double> expectedArrSubVal{111.11, 4321.55, 5555.44};
    std::vector<double> actualArrSubVal{};

    for (auto it = arrSub.begin(); it != arrSub.end(); ++it) {
        ASSERT_TRUE(std::holds_alternative<double>(*it));
        actualArrSubVal.emplace_back(std::get<double>(*it));
    }
    EXPECT_TRUE(std::ranges::equal(expectedArrSubVal, actualArrSubVal));

    // validate array.[].sub2 matches expected values
    EXPECT_TRUE(jsonFileObj.containsKey("array.[].sub2"));
    auto const arrSub2 = jsonFileObj.getArray("array.[].sub2");
    EXPECT_EQ(arrSub2.size(), 3);
    std::vector<std::string> expectedArrSub2Val{"subCategory", "temporary", "london"};
    std::vector<std::string> actualArrSub2Val{};

    for (auto it = arrSub2.begin(); it != arrSub2.end(); ++it) {
        ASSERT_TRUE(std::holds_alternative<std::string>(*it));
        actualArrSub2Val.emplace_back(std::get<std::string>(*it));
    }
    EXPECT_TRUE(std::ranges::equal(expectedArrSub2Val, actualArrSub2Val));

    EXPECT_TRUE(jsonFileObj.containsKey("dosguard.whitelist.[]"));
    auto const whitelistArr = jsonFileObj.getArray("dosguard.whitelist.[]");
    EXPECT_EQ(whitelistArr.size(), 2);
    EXPECT_EQ("125.5.5.1", std::get<std::string>(whitelistArr.at(0)));
    EXPECT_EQ("204.2.2.1", std::get<std::string>(whitelistArr.at(1)));
}

struct JsonValueDeathTest : JsonFromTempFile {};

TEST_F(ConfigValueJsonGetArrayDeathTest, invalidGetValues)
{
    // not possible for json value to call a value that doesn't exist
    EXPECT_DEATH([[maybe_unused]] auto a = jsonFileObj.getArray("header.text1"), ".*");
}

// -------------------------------------------------------------------

struct ConfigFileJsonParseTestBundle {
    using ValidationMap = std::unordered_map<
        std::string,
        std::variant<int64_t, double, bool, std::string, boost::json::object, boost::json::array>>;

    std::string testName;
    std::string configStr;
    ValidationMap validationMap;
};

struct ConfigFileJsonParseTest : NoLoggerFixture, testing::WithParamInterface<ConfigFileJsonParseTestBundle> {};

TEST_P(ConfigFileJsonParseTest, parseValues)
{
    static constexpr auto kEPS = 1e-9;
    ConfigFileJson const configFile{boost::json::parse(GetParam().configStr).as_object()};

    auto const& flatJson = configFile.inner();

    ASSERT_EQ(GetParam().validationMap.size(), flatJson.size());
    std::ranges::for_each(GetParam().validationMap, [&flatJson](auto const& kvPair) {
        auto const& key = kvPair.first;
        auto const& value = kvPair.second;

        EXPECT_TRUE(flatJson.contains(key));

        std::visit(
            util::OverloadSet{
                [&flatJson, &key](int64_t const v) {
                    EXPECT_TRUE(flatJson.at(key).is_number()) << key << ": " << v;
                    EXPECT_EQ(flatJson.at(key).as_int64(), v) << key << ": " << v;
                },
                [&flatJson, &key](double const v) {
                    EXPECT_TRUE(flatJson.at(key).is_double()) << key << ": " << v;
                    EXPECT_NEAR(flatJson.at(key).as_double(), v, kEPS) << key << ": " << v;
                },
                [&flatJson, &key](bool const v) {
                    EXPECT_TRUE(flatJson.at(key).is_bool()) << key << ": " << v;
                    EXPECT_EQ(flatJson.at(key).as_bool(), v) << key << ": " << v;
                },
                [&flatJson, &key](std::string const& v) {
                    EXPECT_TRUE(flatJson.at(key).is_string()) << key << ": " << v;
                    EXPECT_EQ(flatJson.at(key).as_string(), v) << key << ": " << v;
                },
                [&flatJson, &key](boost::json::object const& v) {
                    EXPECT_TRUE(flatJson.at(key).is_object()) << key << ": " << v;
                    EXPECT_EQ(flatJson.at(key).as_object(), v) << key << ": " << v;
                },
                [&flatJson, &key](boost::json::array const& v) {
                    EXPECT_TRUE(flatJson.at(key).is_array()) << key << ": " << v;
                    EXPECT_EQ(flatJson.at(key).as_array(), v) << key << ": " << v;
                },
            },
            value
        );
    });
}

INSTANTIATE_TEST_CASE_P(
    ConfigFileJsonParseTestGroup,
    ConfigFileJsonParseTest,
    testing::Values(
        ConfigFileJsonParseTestBundle{
            .testName = "values",
            .configStr = R"json({
                "int": 42,
                "double": 123.456,
                "bool": true,
                "string": "some string"
            })json",
            .validationMap = {{"int", 42}, {"double", 123.456}, {"bool", true}, {"string", "some string"}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "nested",
            .configStr = R"json({
                "level_0": {
                    "int": 42,
                    "level_1":{
                        "double": 123.456,
                        "level_2": {
                            "bool": true,
                            "level_3": {
                                "string": "some string"
                            }
                        }
                    }
                }
            })json",
            .validationMap =
                {{"level_0.int", 42},
                 {"level_0.level_1.double", 123.456},
                 {"level_0.level_1.level_2.bool", true},
                 {"level_0.level_1.level_2.level_3.string", "some string"}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "array",
            .configStr = R"json({
                "array": [1, 2, 3]
            })json",
            .validationMap = {{"array.[]", boost::json::array{1, 2, 3}}}
        }
    ),
    tests::util::kNAME_GENERATOR
);
