#include <algorithm>
#include <clio/backend/DBHelpers.h>
#include <clio/rpc/RPCHelpers.h>
#include <gtest/gtest.h>

#include <test/env/env.h>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

int
main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}