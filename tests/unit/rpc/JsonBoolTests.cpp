#include "rpc/common/JsonBool.hpp"
#include "util/NameGenerator.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value_to.hpp>
#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

struct JsonBoolTestsCaseBundle {
    std::string testName;
    std::string json;
    bool expectedBool;
};

class JsonBoolTests : public TestWithParam<JsonBoolTestsCaseBundle> {
public:
    static auto
    generateTestValuesForParametersTest()
    {
        return std::vector<JsonBoolTestsCaseBundle>{
            {.testName = "NullValue",
             .json = R"JSON({ "test_bool": null })JSON",
             .expectedBool = false},
            {.testName = "BoolTrueValue",
             .json = R"JSON({ "test_bool": true })JSON",
             .expectedBool = true},
            {.testName = "BoolFalseValue",
             .json = R"JSON({ "test_bool": false })JSON",
             .expectedBool = false},
            {.testName = "IntTrueValue",
             .json = R"JSON({ "test_bool": 1 })JSON",
             .expectedBool = true},
            {.testName = "IntFalseValue",
             .json = R"JSON({ "test_bool": 0 })JSON",
             .expectedBool = false},
            {.testName = "DoubleTrueValue",
             .json = R"JSON({ "test_bool": 0.1 })JSON",
             .expectedBool = true},
            {.testName = "DoubleFalseValue",
             .json = R"JSON({ "test_bool": 0.0 })JSON",
             .expectedBool = false},
            {.testName = "StringTrueValue",
             .json = R"JSON({ "test_bool": "true" })JSON",
             .expectedBool = true},
            {.testName = "StringFalseValue",
             .json = R"JSON({ "test_bool": "false" })JSON",
             .expectedBool = true},
            {.testName = "ArrayTrueValue",
             .json = R"JSON({ "test_bool": [0] })JSON",
             .expectedBool = true},
            {.testName = "ArrayFalseValue",
             .json = R"JSON({ "test_bool": [] })JSON",
             .expectedBool = false},
            {.testName = "ObjectTrueValue",
             .json = R"JSON({ "test_bool": { "key": null } })JSON",
             .expectedBool = true},
            {.testName = "ObjectFalseValue",
             .json = R"JSON({ "test_bool": {} })JSON",
             .expectedBool = false}
        };
    }
};

INSTANTIATE_TEST_CASE_P(
    JsonBoolCheckGroup,
    JsonBoolTests,
    ValuesIn(JsonBoolTests::generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(JsonBoolTests, Parse)
{
    auto const testBundle = GetParam();
    auto const jv = json::parse(testBundle.json).as_object();
    ASSERT_TRUE(jv.contains("test_bool"));
    EXPECT_EQ(testBundle.expectedBool, value_to<JsonBool>(jv.at("test_bool")).value);
}
