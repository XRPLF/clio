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

#include "util/Random.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <vector>

using namespace util;

struct RandomTests : public ::testing::Test {
    static std::vector<int>
    generateRandoms(size_t const numRandoms = 1000)
    {
        std::vector<int> v;
        v.reserve(numRandoms);
        std::ranges::generate_n(std::back_inserter(v), numRandoms, []() { return Random::uniform(0, 1000); });
        return v;
    }
};

TEST_F(RandomTests, Uniform)
{
    std::ranges::for_each(generateRandoms(), [](int const& e) {
        EXPECT_GE(e, 0);
        EXPECT_LE(e, 1000);
    });
}

TEST_F(RandomTests, FixedSeed)
{
    Random::setSeed(42);
    std::vector<int> const v1 = generateRandoms();

    Random::setSeed(42);
    std::vector<int> const v2 = generateRandoms();

    for (auto const& [e1, e2] : std::views::zip(v1, v2)) {
        EXPECT_EQ(e1, e2);
    };
}
