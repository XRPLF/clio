#include "util/Random.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <vector>

using namespace util;

struct RandomTests : public ::testing::Test {
    static std::vector<int>
    generateRandoms(MTRandomGenerator& randomGenerator, size_t const numRandoms = 1000)
    {
        std::vector<int> v;
        v.reserve(numRandoms);
        std::ranges::generate_n(std::back_inserter(v), numRandoms, [&randomGenerator]() {
            return randomGenerator.uniform(0, 1000);
        });
        return v;
    }
};

TEST_F(RandomTests, Uniform)
{
    MTRandomGenerator randomGenerator;
    std::ranges::for_each(generateRandoms(randomGenerator), [](int const& e) {
        EXPECT_GE(e, 0);
        EXPECT_LE(e, 1000);
    });
}

TEST_F(RandomTests, FixedSeed)
{
    MTRandomGenerator randomGenerator;

    randomGenerator.setSeed(42);
    std::vector<int> const v1 = generateRandoms(randomGenerator);

    randomGenerator.setSeed(42);
    std::vector<int> const v2 = generateRandoms(randomGenerator);

    ASSERT_EQ(v1.size(), v2.size());
    for (size_t i = 0; i < v1.size(); ++i) {
        EXPECT_EQ(v1[i], v2[i]);
    };
}
