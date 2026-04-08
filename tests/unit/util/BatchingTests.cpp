#include "util/Batching.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <iterator>
#include <vector>

TEST(BatchingTests, simpleBatch)
{
    std::vector<int> const input{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> output;

    util::forEachBatch(input, 3, [&](auto begin, auto end) {
        std::copy(begin, end, std::back_inserter(output));
        EXPECT_LE(std::distance(begin, end), 3);
    });

    EXPECT_EQ(input, output);
}

TEST(BatchingTests, simpleBatchEven)
{
    std::vector<int> const input{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> output;

    util::forEachBatch(input, 2, [&](auto begin, auto end) {
        std::copy(begin, end, std::back_inserter(output));
        EXPECT_LE(std::distance(begin, end), 2);
    });

    EXPECT_EQ(input, output);
}

TEST(BatchingTests, batchSizeBiggerThanInput)
{
    std::vector<int> const input{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> output;

    util::forEachBatch(input, 20, [&](auto begin, auto end) {
        std::copy(begin, end, std::back_inserter(output));
        EXPECT_LE(std::distance(begin, end), 20);
    });

    EXPECT_EQ(input, output);
}

TEST(BatchingTests, emptyInput)
{
    std::vector<int> const input{};
    std::vector<int> output;

    util::forEachBatch(input, 20, [&](auto begin, auto end) {
        std::copy(begin, end, std::back_inserter(output));
        ASSERT_FALSE(true) << "Should not be called";
    });

    EXPECT_EQ(input, output);
}
