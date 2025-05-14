//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "util/newconfig/ConfigConstraints.hpp"
#include "util/newconfig/Types.hpp"

#include <gtest/gtest.h>

using namespace util::config;

struct RpcNameConstraintTest : testing::Test {};

TEST_F(RpcNameConstraintTest, WrongType)
{
    Value value{1};
    auto const maybeError = gRpcNameConstraint.checkConstraint(value);
    ASSERT_TRUE(maybeError.has_value());
    EXPECT_EQ(maybeError->error, "RPC command name must be a string");
}

TEST_F(RpcNameConstraintTest, WrongValue)
{
    Value value{"non_existing_rpc"};
    auto const maybeError = gRpcNameConstraint.checkConstraint(value);
    ASSERT_TRUE(maybeError.has_value());
    EXPECT_EQ(maybeError->error, "Invalid RPC command name");
}

TEST_F(RpcNameConstraintTest, CorrectValue)
{
    Value value{"server_info"};
    auto const maybeError = gRpcNameConstraint.checkConstraint(value);
    ASSERT_FALSE(maybeError.has_value());
}
