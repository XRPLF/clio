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

#include "util/Fixtures.h"
#include "util/Retry.h"

#include <boost/asio/strand.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>

using namespace util;

struct RetryTests : virtual ::testing::Test {
    std::chrono::milliseconds const delay{1};
    std::chrono::milliseconds const maxDelay{5};
};

TEST_F(RetryTests, ExponentialBackoffStrategy)
{
    ExponentialBackoffStrategy strategy{delay, maxDelay};

    EXPECT_EQ(strategy.getDelay(), delay);
    EXPECT_EQ(strategy.nextDelay(), delay * 2);

    strategy.increaseDelay();
    EXPECT_EQ(strategy.getDelay(), delay * 2);
    EXPECT_EQ(strategy.nextDelay(), delay * 4);

    strategy.increaseDelay();
    EXPECT_LT(strategy.getDelay(), maxDelay);

    for (size_t i = 0; i < 10; ++i) {
        strategy.increaseDelay();
        EXPECT_EQ(strategy.getDelay(), maxDelay);
        EXPECT_EQ(strategy.getDelay(), maxDelay);
    }
}

struct RetryWithExponentialBackoffStrategyTests : SyncAsioContextTest, RetryTests {
    RetryWithExponentialBackoffStrategyTests()
    {
        EXPECT_EQ(retry.attemptNumber(), 0);
        EXPECT_EQ(retry.currentDelay(), delay);
        EXPECT_EQ(retry.nextDelay(), delay * 2);
    }

    Retry retry = makeRetryExponentialBackoff(delay, maxDelay, boost::asio::make_strand(ctx));
    testing::MockFunction<void()> mockCallback;
};

TEST_F(RetryWithExponentialBackoffStrategyTests, Retry)
{
    retry.retry(mockCallback.AsStdFunction());

    EXPECT_EQ(retry.attemptNumber(), 0);
    EXPECT_EQ(retry.currentDelay(), delay * 2);

    EXPECT_CALL(mockCallback, Call());
    runContext();

    EXPECT_EQ(retry.attemptNumber(), 1);
}

TEST_F(RetryWithExponentialBackoffStrategyTests, Cancel)
{
    retry.retry(mockCallback.AsStdFunction());
    retry.cancel();
    runContext();
    EXPECT_EQ(retry.attemptNumber(), 0);

    retry.cancel();
    EXPECT_EQ(retry.attemptNumber(), 0);
}
