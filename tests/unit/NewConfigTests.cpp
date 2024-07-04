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
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Object.hpp"

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <gtest/gtest.h>

#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

using namespace util::config;

struct NewConfigTest : NoLoggerFixture {};

static Object configData = {Object{
    {"header.text1", ConfigValue{ConfigType::String}.defaultValue("value")},
    {"header.port", ConfigValue{ConfigType::Integer}.defaultValue(123)},
    {"header.Admin", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
    {"ip", ConfigValue{ConfigType::Double}.defaultValue(444.22)}
}};

TEST_F(NewConfigTest, ConfigValueTest)
{
    ConfigValue cv = ConfigValue{ConfigType::String}.defaultValue("value");
    ASSERT_EQ("value", cv.asString());
    ASSERT_EQ(ConfigType::String, cv.type());
    ASSERT_THROW(cv.asBool(), std::bad_variant_access);
    ASSERT_THROW(cv.asDouble(), std::bad_variant_access);
    ASSERT_THROW(cv.asInt(), std::bad_variant_access);
}

TEST_F(NewConfigTest, ObjectValueTest)
{
    ASSERT_EQ("value", configData.getValue("header.text1").asString());
    ASSERT_EQ(123, configData.getValue("header.port").asInt());
    ASSERT_EQ(true, configData.getValue("header.Admin").asBool());
    ASSERT_EQ(444.22, configData.getValue("ip").asDouble());
}

// always, an array would only hold one type of value, but here we test multiple
constexpr static auto JSONData = R"JSON(
    {
        "computers": [                
            { "port": 1234 },
            { "admin": true },
            { "CompTypes": [{ "MSI": "1000" }, {"htp": 34.5}]}
        ],
        "section": {
            "test": {
                "str": "hello",
                "int": 5554,
                "bool": true,
                "double": 3.14
            }
        },
        "top": 999
    }
)JSON";

class JsonTest : public NoLoggerFixture {
protected:
    ConfigFileJson obj{boost::json::parse(JSONData).as_object()};
};

TEST_F(JsonTest, parseJsonTestArray)
{
    ASSERT_EQ(999, obj.getValue("top")->asInt());
    std::optional<std::vector<ConfigFileJson::configVal>> arr = obj.getArray("computers");
    ASSERT_TRUE(arr.has_value());

    for (auto const& pair : arr.value()) {
        auto const key = pair.first;
        auto const value = pair.second;

        if (value.type() == ConfigType::Integer) {
            ASSERT_EQ("port", key);
            ASSERT_EQ(1234, value.asInt());
        } else if (value.type() == ConfigType::Boolean) {
            ASSERT_EQ("admin", key);
            ASSERT_EQ(true, value.asBool());
        } else if (value.type() == ConfigType::String) {
            ASSERT_EQ("CompTypes.MSI", key);
            // ASSERT_EQ("1000", value.asString());
        } else if (value.type() == ConfigType::Double) {
            ASSERT_EQ("CompTypes.htp", key);
            ASSERT_EQ(34.5, value.asDouble());
        }
    }
}
