#include "util/config/ConfigConstraints.hpp"
#include "util/config/Types.hpp"

#include <gtest/gtest.h>

using namespace util::config;

struct RpcNameConstraintTest : testing::Test {};

TEST_F(RpcNameConstraintTest, WrongType)
{
    Value const value{1};
    auto const maybeError = gRpcNameConstraint.checkConstraint(value);
    ASSERT_TRUE(maybeError.has_value());
    EXPECT_EQ(maybeError->error, "RPC command name must be a string");
}

TEST_F(RpcNameConstraintTest, WrongValue)
{
    Value const value{"non_existing_rpc"};
    auto const maybeError = gRpcNameConstraint.checkConstraint(value);
    ASSERT_TRUE(maybeError.has_value());
    EXPECT_EQ(maybeError->error, "Invalid RPC command name");
}

TEST_F(RpcNameConstraintTest, CorrectValue)
{
    Value const value{"server_info"};
    auto const maybeError = gRpcNameConstraint.checkConstraint(value);
    ASSERT_FALSE(maybeError.has_value());
}
