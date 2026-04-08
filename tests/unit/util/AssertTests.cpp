#include "util/Assert.hpp"
#include "util/MockAssert.hpp"

#include <gtest/gtest.h>

struct AssertTest : common::util::WithMockAssert {};

TEST_F(AssertTest, assertTrue)
{
    EXPECT_NO_THROW(ASSERT(true, "Should not fail"));
}

TEST_F(AssertTest, assertFalse)
{
    EXPECT_CLIO_ASSERT_FAIL({ ASSERT(false, "failure"); });
}
