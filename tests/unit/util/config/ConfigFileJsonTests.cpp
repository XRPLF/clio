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
#include "util/NameGenerator.hpp"
#include "util/OverloadSet.hpp"
#include "util/TmpFile.hpp"
#include "util/config/ConfigFileJson.hpp"

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

using namespace util::config;

namespace {
constexpr auto kEPS = 1e-9;
}  // namespace

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
            .configStr = R"JSON({
                "int": 42,
                "double": 123.456,
                "bool": true,
                "string": "some string"
            })JSON",
            .validationMap = {{"int", 42}, {"double", 123.456}, {"bool", true}, {"string", "some string"}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "nested",
            .configStr = R"JSON({
                "level_0": {
                    "int": 42,
                    "level_1": {
                        "double": 123.456,
                        "level_2": {
                            "bool": true,
                            "level_3": {
                                "string": "some string"
                            }
                        }
                    }
                }
            })JSON",
            .validationMap =
                {{"level_0.int", 42},
                 {"level_0.level_1.double", 123.456},
                 {"level_0.level_1.level_2.bool", true},
                 {"level_0.level_1.level_2.level_3.string", "some string"}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "array",
            .configStr = R"JSON({
                "array": [1, 2, 3]
            })JSON",
            .validationMap = {{"array.[]", boost::json::array{1, 2, 3}}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "nested_array",
            .configStr = R"JSON({
                "level_0": {
                    "array": [1, 2, 3],
                    "level_1": {
                        "array": [4, 5, 6],
                        "level_2": {
                            "array": [7, 8, 9]
                        }
                    }
                }
            })JSON",
            .validationMap =
                {
                    {"level_0.array.[]", boost::json::array{1, 2, 3}},
                    {"level_0.level_1.array.[]", boost::json::array{4, 5, 6}},
                    {"level_0.level_1.level_2.array.[]", boost::json::array{7, 8, 9}},
                }
        },
        ConfigFileJsonParseTestBundle{
            .testName = "mixed",
            .configStr = R"JSON({
                "int": 42,
                "double": 123.456,
                "bool": true,
                "string": "some string",
                "array": [1, 2, 3],
                "nested": {
                    "int": 42,
                    "double": 123.456,
                    "bool": true,
                    "string": "some string",
                    "array": [1, 2, 3]
                }
            })JSON",
            .validationMap =
                {
                    {"int", 42},
                    {"double", 123.456},
                    {"bool", true},
                    {"string", "some string"},
                    {"array.[]", boost::json::array{1, 2, 3}},
                    {"nested.int", 42},
                    {"nested.double", 123.456},
                    {"nested.bool", true},
                    {"nested.string", "some string"},
                    {"nested.array.[]", boost::json::array{1, 2, 3}},
                }
        },
        ConfigFileJsonParseTestBundle{.testName = "empty", .configStr = R"JSON({})JSON", .validationMap = {}},
        ConfigFileJsonParseTestBundle{
            .testName = "empty_nested",
            .configStr = R"JSON({
                "level_0": {
                    "level_1": {
                        "level_2": {
                            "level_3": {}
                        }
                    }
                }
            })JSON",
            .validationMap = {}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "empty_array",
            .configStr = R"JSON({
                "array": []
            })JSON",
            .validationMap = {{"array.[]", boost::json::array{}}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "empty_nested_array",
            .configStr = R"JSON({
                "level_0": {
                    "array": [],
                    "level_1": {
                        "array": [],
                        "level_2": {
                            "array": []
                        }
                    }
                }
            })JSON",
            .validationMap =
                {
                    {"level_0.array.[]", boost::json::array{}},
                    {"level_0.level_1.array.[]", boost::json::array{}},
                    {"level_0.level_1.level_2.array.[]", boost::json::array{}},
                }
        },
        ConfigFileJsonParseTestBundle{
            .testName = "object_inside_array",
            .configStr = R"JSON({
                "array": [
                    { "int": 42 }
                ]
            })JSON",
            .validationMap = {{"array.[].int", boost::json::array{42}}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "object_with_optional_fields_inside_array",
            .configStr = R"JSON({
                "array": [
                    {"int": 42},
                    {"int": 24, "bool": true}
                ]
            })JSON",
            .validationMap =
                {{"array.[].int", boost::json::array{42, 24}},
                 {"array.[].bool", boost::json::array{boost::json::value{}, true}}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "full_object_is_at_the_front_of_array",
            .configStr = R"JSON({
                "array": [
                    {"int": 42, "bool": true},
                    {"int": 2},
                    {"int": 4}
                ]
            })JSON",
            .validationMap =
                {{"array.[].int", boost::json::array{42, 2, 4}},
                 {"array.[].bool", boost::json::array{true, boost::json::value{}, boost::json::value{}}}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "full_object_is_in_the_middle_of_array",
            .configStr = R"JSON({
                "array": [
                    {"int": 42},
                    {"int": 2, "bool": true},
                    {"int": 4}
                ]
            })JSON",
            .validationMap =
                {{"array.[].int", boost::json::array{42, 2, 4}},
                 {"array.[].bool", boost::json::array{boost::json::value{}, true, boost::json::value{}}}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "no_full_object",
            .configStr = R"JSON({
                "array": [
                    {"int": 42},
                    {"int": 2},
                    {"bool": true}
                ]
            })JSON",
            .validationMap =
                {{"array.[].int", boost::json::array{42, 2, boost::json::value{}}},
                 {"array.[].bool", boost::json::array{boost::json::value{}, boost::json::value{}, true}}}
        },
        ConfigFileJsonParseTestBundle{
            .testName = "array_with_nexted_objects",
            .configStr = R"JSON({
                "array": [
                    { "object": { "int": 42 } },
                    { "object": { "string": "some string" } }
                ]
            })JSON",
            .validationMap =
                {{"array.[].object.int", boost::json::array{42, boost::json::value{}}},
                 {"array.[].object.string", boost::json::array{boost::json::value{}, "some string"}}}
        }
    ),
    tests::util::kNAME_GENERATOR
);

struct ConfigFileJsonTest : NoLoggerFixture {};

TEST_F(ConfigFileJsonTest, getValue)
{
    auto const jsonStr = R"JSON({
        "int": 42,
        "object": { "string": "some string" },
        "bool": true,
        "double": 123.456
    })JSON";
    auto const jsonFileObj = ConfigFileJson{boost::json::parse(jsonStr).as_object()};

    auto const intValue = jsonFileObj.getValue("int");
    ASSERT_TRUE(std::holds_alternative<int64_t>(intValue));
    EXPECT_EQ(std::get<int64_t>(intValue), 42);

    auto const stringValue = jsonFileObj.getValue("object.string");
    ASSERT_TRUE(std::holds_alternative<std::string>(stringValue));
    EXPECT_EQ(std::get<std::string>(stringValue), "some string");

    auto const boolValue = jsonFileObj.getValue("bool");
    ASSERT_TRUE(std::holds_alternative<bool>(boolValue));
    EXPECT_EQ(std::get<bool>(boolValue), true);

    auto const doubleValue = jsonFileObj.getValue("double");
    ASSERT_TRUE(std::holds_alternative<double>(doubleValue));
    EXPECT_NEAR(std::get<double>(doubleValue), 123.456, kEPS);

    EXPECT_FALSE(jsonFileObj.containsKey("object.int"));
}

struct ConfigFileJsonAssertTest : common::util::WithMockAssert, ConfigFileJsonTest {};

TEST_F(ConfigFileJsonAssertTest, getValueInvalidKey)
{
    auto const jsonFileObj = ConfigFileJson{boost::json::parse("{}").as_object()};
    EXPECT_CLIO_ASSERT_FAIL([[maybe_unused]] auto a = jsonFileObj.getValue("some_key"));
}

TEST_F(ConfigFileJsonAssertTest, getValueOfArray)
{
    auto const jsonStr = R"JSON({
        "array": [1, 2, 3]
    })JSON";
    auto const jsonFileObj = ConfigFileJson{boost::json::parse(jsonStr).as_object()};
    EXPECT_CLIO_ASSERT_FAIL([[maybe_unused]] auto a = jsonFileObj.getValue("array"));
}

TEST_F(ConfigFileJsonAssertTest, nullIsNotSupported)
{
    auto const jsonStr = R"JSON({
        "null": null
    })JSON";
    auto const jsonFileObj = ConfigFileJson{boost::json::parse(jsonStr).as_object()};
    EXPECT_CLIO_ASSERT_FAIL([[maybe_unused]] auto a = jsonFileObj.getValue("null"));
}

TEST_F(ConfigFileJsonTest, getArray)
{
    auto const jsonStr = R"JSON({
        "array": [1, "2", 3.14, true],
        "object": { "array": [3, 4] }
    })JSON";
    auto const jsonFileObj = ConfigFileJson{boost::json::parse(jsonStr).as_object()};

    auto const array = jsonFileObj.getArray("array.[]");
    ASSERT_EQ(array.size(), 4);

    auto const value0 = array.at(0).value();
    ASSERT_TRUE(std::holds_alternative<int64_t>(value0));
    EXPECT_EQ(std::get<int64_t>(value0), 1);

    auto const value1 = array.at(1).value();
    ASSERT_TRUE(std::holds_alternative<std::string>(value1));
    EXPECT_EQ(std::get<std::string>(value1), "2");

    auto const value2 = array.at(2).value();
    ASSERT_TRUE(std::holds_alternative<double>(value2));
    EXPECT_NEAR(std::get<double>(value2), 3.14, kEPS);

    auto const value3 = array.at(3).value();
    ASSERT_TRUE(std::holds_alternative<bool>(value3));
    EXPECT_EQ(std::get<bool>(value3), true);

    auto const arrayFromObject = jsonFileObj.getArray("object.array.[]");
    ASSERT_EQ(arrayFromObject.size(), 2);
    EXPECT_EQ(std::get<int64_t>(arrayFromObject.at(0).value()), 3);
    EXPECT_EQ(std::get<int64_t>(arrayFromObject.at(1).value()), 4);
}

TEST_F(ConfigFileJsonTest, getArrayObjectInArray)
{
    auto const jsonStr = R"JSON({
        "array": [
            { "int": 42 },
            { "string": "some string" }
        ]
    })JSON";
    auto const jsonFileObj = ConfigFileJson{boost::json::parse(jsonStr).as_object()};

    auto const ints = jsonFileObj.getArray("array.[].int");
    ASSERT_EQ(ints.size(), 2);
    ASSERT_TRUE(std::holds_alternative<int64_t>(ints.at(0).value()));
    EXPECT_EQ(std::get<int64_t>(ints.at(0).value()), 42);
    EXPECT_FALSE(ints.at(1).has_value());

    auto const strings = jsonFileObj.getArray("array.[].string");
    ASSERT_EQ(strings.size(), 2);
    EXPECT_FALSE(strings.at(0).has_value());
    ASSERT_TRUE(std::holds_alternative<std::string>(strings.at(1).value()));
    EXPECT_EQ(std::get<std::string>(strings.at(1).value()), "some string");
}

TEST_F(ConfigFileJsonTest, getArrayOptionalInArray)
{
    auto const jsonStr = R"JSON({
        "array": [
            { "int": 42 },
            { "int": 24, "bool": true }
        ]
    })JSON";
    auto const jsonFileObj = ConfigFileJson{boost::json::parse(jsonStr).as_object()};

    auto const ints = jsonFileObj.getArray("array.[].int");
    ASSERT_EQ(ints.size(), 2);
    ASSERT_TRUE(std::holds_alternative<int64_t>(ints.at(0).value()));
    EXPECT_EQ(std::get<int64_t>(ints.at(0).value()), 42);
    ASSERT_TRUE(std::holds_alternative<int64_t>(ints.at(1).value()));
    EXPECT_EQ(std::get<int64_t>(ints.at(1).value()), 24);

    auto const bools = jsonFileObj.getArray("array.[].bool");
    ASSERT_EQ(bools.size(), 2);
    EXPECT_FALSE(bools.at(0).has_value());
    ASSERT_TRUE(std::holds_alternative<bool>(bools.at(1).value()));
    EXPECT_EQ(std::get<bool>(bools.at(1).value()), true);
}

TEST_F(ConfigFileJsonAssertTest, getArrayInvalidKey)
{
    auto const jsonFileObj = ConfigFileJson{boost::json::parse("{}").as_object()};
    EXPECT_CLIO_ASSERT_FAIL([[maybe_unused]] auto a = jsonFileObj.getArray("some_key"));
}

TEST_F(ConfigFileJsonAssertTest, getArrayNotArray)
{
    auto const jsonStr = R"JSON({
        "int": 42
    })JSON";
    auto const jsonFileObj = ConfigFileJson{boost::json::parse(jsonStr).as_object()};
    EXPECT_CLIO_ASSERT_FAIL([[maybe_unused]] auto a = jsonFileObj.getArray("int"));
}

TEST_F(ConfigFileJsonTest, containsKey)
{
    auto const jsonStr = R"JSON({
        "int": 42,
        "object": { "string": "some string", "array": [1, 2, 3] },
        "array2": [1, 2, 3],
        "array_of_objects": [ {"int": 42}, {"string": "some string"} ]
    })JSON";
    auto const jsonFileObj = ConfigFileJson{boost::json::parse(jsonStr).as_object()};

    EXPECT_TRUE(jsonFileObj.containsKey("int"));
    EXPECT_FALSE(jsonFileObj.containsKey("other_key"));

    EXPECT_TRUE(jsonFileObj.containsKey("object.string"));
    EXPECT_FALSE(jsonFileObj.containsKey("object.int"));
    EXPECT_TRUE(jsonFileObj.containsKey("object.array.[]"));
    EXPECT_FALSE(jsonFileObj.containsKey("object.array"));

    EXPECT_TRUE(jsonFileObj.containsKey("array2.[]"));
    EXPECT_FALSE(jsonFileObj.containsKey("array2"));
    EXPECT_FALSE(jsonFileObj.containsKey("array2.[].int"));

    EXPECT_TRUE(jsonFileObj.containsKey("array_of_objects.[].int"));
    EXPECT_TRUE(jsonFileObj.containsKey("array_of_objects.[].string"));
    EXPECT_FALSE(jsonFileObj.containsKey("array_of_objects.[]"));
    EXPECT_FALSE(jsonFileObj.containsKey("array_of_objects.[].object"));
}

TEST_F(ConfigFileJsonTest, getAllKeys)
{
    auto const jsonStr = R"JSON({
        "int": 42,
        "object": { "string": "some string", "array": [1, 2, 3] },
        "array2": [1, 2, 3],
        "array_of_objects": [ {"int": 42}, {"string": "some string"} ]
    })JSON";
    auto const jsonFileObj = ConfigFileJson{boost::json::parse(jsonStr).as_object()};

    auto allKeys = jsonFileObj.getAllKeys();
    std::ranges::sort(allKeys);
    EXPECT_EQ(allKeys.size(), 6);

    std::vector<std::string> const expectedKeys{
        {"array2.[]",
         "array_of_objects.[].int",
         "array_of_objects.[].string",
         "int",
         "object.array.[]",
         "object.string"}
    };
    EXPECT_EQ(allKeys, expectedKeys);
}

struct ConfigFileJsonMakeTest : ConfigFileJsonTest {};

TEST_F(ConfigFileJsonMakeTest, invalidFile)
{
    auto const jsonFileObj = ConfigFileJson::makeConfigFileJson("does_not_exist");
    EXPECT_FALSE(jsonFileObj.has_value());
}

TEST_F(ConfigFileJsonMakeTest, invalidJson)
{
    auto const file = TmpFile("invalid json");
    auto const jsonFileObj = ConfigFileJson::makeConfigFileJson(file.path);
    EXPECT_FALSE(jsonFileObj.has_value());
}

TEST_F(ConfigFileJsonMakeTest, validFile)
{
    auto const file = TmpFile(R"JSON({ "int": 42 })JSON");
    auto const jsonFileObj = ConfigFileJson::makeConfigFileJson(file.path);
    ASSERT_TRUE(jsonFileObj.has_value());

    auto const& flatJson = jsonFileObj->inner();
    ASSERT_EQ(flatJson.size(), 1);
    ASSERT_TRUE(flatJson.contains("int"));
    ASSERT_TRUE(flatJson.at("int").is_number());
    EXPECT_EQ(flatJson.at("int").as_int64(), 42);
}
