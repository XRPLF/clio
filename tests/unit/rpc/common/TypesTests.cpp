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
#include "rpc/common/Types.hpp"

#include <boost/json/array.hpp>
#include <boost/json/value.hpp>
#include <gtest/gtest.h>

#include <expected>

using namespace rpc;

TEST(MaybeErrorTest, OperatorEquals)
{
    EXPECT_EQ(MaybeError{}, MaybeError{});
    EXPECT_NE(MaybeError{}, MaybeError{std::unexpected{Status{"Error"}}});
    EXPECT_NE(MaybeError{std::unexpected{Status{"Error"}}}, MaybeError{});
    EXPECT_EQ(MaybeError{std::unexpected{Status{"Error"}}}, MaybeError{std::unexpected{Status{"Error"}}});
    EXPECT_NE(MaybeError{std::unexpected{Status{"Error"}}}, MaybeError{std::unexpected{Status{"Another_error"}}});
}

TEST(ReturnTypeTests, Constructor)
{
    boost::json::value const value{42};

    {
        ReturnType const r{value};
        ASSERT_TRUE(r.result);
        EXPECT_EQ(r.result.value(), value);
        EXPECT_EQ(r.warnings, boost::json::array{});
    }

    {
        boost::json::array const warnings{1, 2, 3};
        ReturnType const r{value, warnings};
        ASSERT_TRUE(r.result);
        EXPECT_EQ(r.result.value(), value);
        EXPECT_EQ(r.warnings, warnings);
    }

    {
        Status const status{"Error"};

        ReturnType const r{std::unexpected{status}};
        ASSERT_FALSE(r.result);
        EXPECT_EQ(r.result.error(), status);
        EXPECT_EQ(r.warnings, boost::json::array{});
    }
}

TEST(ReturnTypeTests, operatorBool)
{
    {
        boost::json::value const value{42};
        ReturnType const r{value};
        EXPECT_TRUE(r);
    }
    {
        Status const status{"Error"};
        ReturnType const r{std::unexpected{status}};
        EXPECT_FALSE(r);
    }
}
