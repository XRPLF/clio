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
    ASSERT_TRUE(warning);
    EXPECT_EQ(warning->warningCode, WarningCode::warnRPC_DEPRECATED);

    warning = Deprecated<>::check(json, "other");
    EXPECT_FALSE(warning);
}

TEST_F(DeprecatedTests, FieldWithStringValue)
{
    Deprecated<std::string> checker{"some_value"};
    auto warning = checker.check(json, "some_string");
    ASSERT_TRUE(warning);
    EXPECT_EQ(warning->warningCode, WarningCode::warnRPC_DEPRECATED);
    EXPECT_EQ(warning->extraMessage, "Value 'some_value' for field 'some_string' is deprecated");
    EXPECT_FALSE(Deprecated<std::string>{"other"}.check(json, "some_string"));
}

TEST_F(DeprecatedTests, FieldWithIntValue)
{
    Deprecated<int> checker{42};
    auto warning = checker.check(json, "some_number");
    ASSERT_TRUE(warning);
    EXPECT_EQ(warning->warningCode, WarningCode::warnRPC_DEPRECATED);
    EXPECT_EQ(warning->extraMessage, "Value '42' for field 'some_number' is deprecated");
    EXPECT_FALSE(Deprecated<int>{43}.check(json, "some_number"));
}

TEST_F(DeprecatedTests, FieldWithBoolValue)
{
    Deprecated<bool> checker{false};
    auto warning = checker.check(json, "some_bool");
    ASSERT_TRUE(warning);
    EXPECT_EQ(warning->warningCode, WarningCode::warnRPC_DEPRECATED);
    EXPECT_EQ(warning->extraMessage, "Value 'false' for field 'some_bool' is deprecated");
    EXPECT_FALSE(Deprecated<bool>{true}.check(json, "some_bool"));
}

TEST_F(DeprecatedTests, FieldWithFloatValue)
{
    Deprecated<float> checker{3.14};
    auto warning = checker.check(json, "some_float");
    ASSERT_TRUE(warning);
    EXPECT_EQ(warning->warningCode, WarningCode::warnRPC_DEPRECATED);
    EXPECT_EQ(warning->extraMessage, "Value '3.14' for field 'some_float' is deprecated");
    EXPECT_FALSE(Deprecated<float>{3.15}.check(json, "some_float"));
}
