#include "rpc/Errors.hpp"
#include "web/LoadWarning.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using namespace web;

namespace {

struct LoadWarningNonJsonTest : testing::TestWithParam<std::string> {};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    NonJsonBodies,
    LoadWarningNonJsonTest,
    testing::Values(
        "Unable to parse JSON from the request",
        "Null method",
        "method is empty",
        "method is not string",
        "params unparsable",
        "Bad target",
        "Too many requests for one connection",
        "",
        "[1, 2, 3]",  // valid JSON, but not an object
        "42",
        "null",
        R"("a string")"
    )
);

TEST_P(LoadWarningNonJsonTest, ReturnsNulloptInsteadOfThrowing)
{
    EXPECT_NO_THROW({ EXPECT_FALSE(withLoadWarning(GetParam()).has_value()); });
}

TEST(LoadWarningTest, AddsWarningToJsonObjectWithoutExistingWarnings)
{
    auto const result = withLoadWarning(R"({"result": {"status": "success"}})");

    ASSERT_TRUE(result.has_value());
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->at("warning").as_string(), "load");
    ASSERT_EQ(result->at("warnings").as_array().size(), 1);
    EXPECT_EQ(
        result->at("warnings").as_array().at(0).as_object().at("id").as_int64(),
        static_cast<int64_t>(rpc::WarningCode::WarnRpcRateLimit)
    );
    EXPECT_EQ(result->at("result").as_object().at("status").as_string(), "success");
    // NOLINTEND(bugprone-unchecked-optional-access)
}

TEST(LoadWarningTest, AppendsToExistingWarningsArray)
{
    auto const result =
        withLoadWarning(R"({"warnings": [{"id": 2001, "message": "already here"}]})");

    ASSERT_TRUE(result.has_value());
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    EXPECT_EQ(result->at("warning").as_string(), "load");
    ASSERT_EQ(result->at("warnings").as_array().size(), 2);
    EXPECT_EQ(result->at("warnings").as_array().at(0).as_object().at("id").as_int64(), 2001);
    EXPECT_EQ(
        result->at("warnings").as_array().at(1).as_object().at("id").as_int64(),
        static_cast<int64_t>(rpc::WarningCode::WarnRpcRateLimit)
    );
    // NOLINTEND(bugprone-unchecked-optional-access)
}

TEST(LoadWarningTest, ReplacesNonArrayWarningsField)
{
    auto const result = withLoadWarning(R"({"warnings": "not an array"})");

    ASSERT_TRUE(result.has_value());
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    ASSERT_TRUE(result->at("warnings").is_array());
    ASSERT_EQ(result->at("warnings").as_array().size(), 1);
    EXPECT_EQ(
        result->at("warnings").as_array().at(0).as_object().at("id").as_int64(),
        static_cast<int64_t>(rpc::WarningCode::WarnRpcRateLimit)
    );
    // NOLINTEND(bugprone-unchecked-optional-access)
}
