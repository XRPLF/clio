//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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
#include <rpc/common/JsonBool.h>

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

struct JsonBoolTestsCaseBundle
{
    std::string testName;
    std::string json;
    bool expectedBool;
};

class JsonBoolTests : public TestWithParam<JsonBoolTestsCaseBundle>
{
public:
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(testing::TestParamInfo<ParamType> const& info) const
        {
            auto bundle = static_cast<JsonBoolTestsCaseBundle>(info.param);
            return bundle.testName;
        }
    };

    static auto
    generateTestValuesForParametersTest()
    {
        return std::vector<JsonBoolTestsCaseBundle>{
            {"NullValue", R"({ "test_bool": null })", false},
            {"BoolTrueValue", R"({ "test_bool": true })", true},
            {"BoolFalseValue", R"({ "test_bool": false })", false},
            {"IntTrueValue", R"({ "test_bool": 1 })", true},
            {"IntFalseValue", R"({ "test_bool": 0 })", false},
            {"DoubleTrueValue", R"({ "test_bool": 0.1 })", true},
            {"DoubleFalseValue", R"({ "test_bool": 0.0 })", false},
            {"StringTrueValue", R"({ "test_bool": "true" })", true},
            {"StringFalseValue", R"({ "test_bool": "false" })", true},
            {"ArrayTrueValue", R"({ "test_bool": [0] })", true},
            {"ArrayFalseValue", R"({ "test_bool": [] })", false},
            {"ObjectTrueValue", R"({ "test_bool": { "key": null } })", true},
            {"ObjectFalseValue", R"({ "test_bool": {} })", false}};
    }
};

INSTANTIATE_TEST_CASE_P(
    JsonBoolCheckGroup,
    JsonBoolTests,
    ValuesIn(JsonBoolTests::generateTestValuesForParametersTest()),
    JsonBoolTests::NameGenerator{});

TEST_P(JsonBoolTests, Parse)
{
    auto const testBundle = GetParam();
    auto const jv = json::parse(testBundle.json).as_object();
    ASSERT_TRUE(jv.contains("test_bool"));
    EXPECT_EQ(testBundle.expectedBool, value_to<JsonBool>(jv.at("test_bool")).value);
}
