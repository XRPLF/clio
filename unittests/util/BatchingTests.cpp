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

#include "util/Batching.h"

#include <gtest/gtest.h>

#include <algorithm>

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
