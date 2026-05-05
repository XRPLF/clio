#include "rpc/Errors.hpp"
#include "rpc/common/Checkers.hpp"

#include <boost/json/value.hpp>
#include <gtest/gtest.h>

#include <string>

using namespace rpc;
using namespace rpc::check;

struct DeprecatedTests : ::testing::Test {
    boost::json::value const json{
        {"some_string", "some_value"},
        {"some_number", 42},
        {"some_bool", false},
        {"some_float", 3.14}
    };
};

TEST_F(DeprecatedTests, Field)
{
    auto warning = Deprecated<>::check(json, "some_string");
    ASSERT_TRUE(warning.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(warning->warningCode, WarningCode::WarnRpcDeprecated);

    warning = Deprecated<>::check(json, "other");
    EXPECT_FALSE(warning.has_value());
}

TEST_F(DeprecatedTests, FieldWithStringValue)
{
    Deprecated<std::string> const checker{"some_value"};
    auto warning = checker.check(json, "some_string");
    ASSERT_TRUE(warning.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(warning->warningCode, WarningCode::WarnRpcDeprecated);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(warning->extraMessage, "Value 'some_value' for field 'some_string' is deprecated");
    EXPECT_FALSE(Deprecated<std::string>{"other"}.check(json, "some_string"));
}

TEST_F(DeprecatedTests, FieldWithIntValue)
{
    Deprecated<int> const checker{42};
    auto warning = checker.check(json, "some_number");
    ASSERT_TRUE(warning.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(warning->warningCode, WarningCode::WarnRpcDeprecated);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(warning->extraMessage, "Value '42' for field 'some_number' is deprecated");
    EXPECT_FALSE(Deprecated<int>{43}.check(json, "some_number"));
}

TEST_F(DeprecatedTests, FieldWithBoolValue)
{
    Deprecated<bool> const checker{false};
    auto warning = checker.check(json, "some_bool");
    ASSERT_TRUE(warning.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(warning->warningCode, WarningCode::WarnRpcDeprecated);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(warning->extraMessage, "Value 'false' for field 'some_bool' is deprecated");
    EXPECT_FALSE(Deprecated<bool>{true}.check(json, "some_bool"));
}

TEST_F(DeprecatedTests, FieldWithFloatValue)
{
    Deprecated<float> const checker{3.14};
    auto warning = checker.check(json, "some_float");
    ASSERT_TRUE(warning.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(warning->warningCode, WarningCode::WarnRpcDeprecated);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(warning->extraMessage, "Value '3.14' for field 'some_float' is deprecated");
    EXPECT_FALSE(Deprecated<float>{3.15}.check(json, "some_float"));
}
