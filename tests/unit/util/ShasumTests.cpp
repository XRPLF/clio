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

#include "util/Shasum.hpp"

#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>

#include <cstdint>
#include <string>
#include <utility>

using namespace util;

struct ShasumTest : testing::Test {
    static constexpr auto kEMPTY_HASH =
        "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";
    static constexpr auto kHELLO_WORLD_HASH =
        "B94D27B9934D3E08A52E52D7DA7DABFAC484EFE37A5380EE9088F7ACE2EFCDE9";
};

TEST_F(ShasumTest, sha256sum)
{
    ripple::uint256 expected;

    ASSERT_TRUE(expected.parseHex(kEMPTY_HASH));
    EXPECT_EQ(sha256sum(""), expected);

    ASSERT_TRUE(expected.parseHex(kHELLO_WORLD_HASH));
    EXPECT_EQ(sha256sum("hello world"), expected);
}

TEST_F(ShasumTest, sha256sumString)
{
    EXPECT_EQ(sha256sumString(""), kEMPTY_HASH);
    EXPECT_EQ(sha256sumString("hello world"), kHELLO_WORLD_HASH);
}

TEST_F(ShasumTest, Sha256sumStreamingEmpty)
{
    Sha256sum hasher;
    auto result = std::move(hasher).finalize();

    ripple::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kEMPTY_HASH));
    EXPECT_EQ(result, expected);
}

TEST_F(ShasumTest, Sha256sumStreamingSingleUpdate)
{
    Sha256sum hasher;
    std::string data = "hello world";
    hasher.update(data.data(), data.size());
    auto result = std::move(hasher).finalize();

    ripple::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHELLO_WORLD_HASH));
    EXPECT_EQ(result, expected);
}

TEST_F(ShasumTest, Sha256sumStreamingMultipleUpdates)
{
    Sha256sum hasher;
    hasher.update("hello", 5);
    hasher.update(" ", 1);
    hasher.update("world", 5);
    auto result = std::move(hasher).finalize();

    ripple::uint256 expected;
    ASSERT_TRUE(expected.parseHex(kHELLO_WORLD_HASH));
    EXPECT_EQ(result, expected);
}

TEST_F(ShasumTest, Sha256sumUpdateTemplate)
{
    Sha256sum hasher;

    uint32_t value32 = 0x12345678;
    uint64_t value64 = 0x123456789ABCDEF0;

    hasher.update(value32);
    hasher.update(value64);

    auto result1 = std::move(hasher).finalize();

    // Verify same result with raw data
    Sha256sum hasher2;
    hasher2.update(&value32, sizeof(value32));
    hasher2.update(&value64, sizeof(value64));

    auto result2 = std::move(hasher2).finalize();

    EXPECT_EQ(result1, result2);
}
