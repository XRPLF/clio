#include "util/BytesConverter.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

using namespace util;

TEST(MBToBytesTest, SimpleValues)
{
    EXPECT_EQ(mbToBytes(0), 0);
    EXPECT_EQ(mbToBytes(1), 1024 * 1024);
    EXPECT_EQ(mbToBytes(2), 2 * 1024 * 1024);
}

TEST(MBToBytesTest, LimitValues)
{
    auto const maxNum = std::numeric_limits<std::uint32_t>::max();
    EXPECT_NE(mbToBytes(maxNum), maxNum * 1024 * 1024);
    EXPECT_EQ(mbToBytes(maxNum), maxNum * 1024ul * 1024ul);
}
