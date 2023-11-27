//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <boost/asio/io_context.hpp>
#include "util/Fixtures.h"
#include <atomic>
#include <cassandra.h>
#include <optional>

#include "data/cassandra/Error.h"
#include "data/cassandra/impl/RetryPolicy.h"

#include <gtest/gtest.h>

using namespace data::cassandra;
using namespace data::cassandra::detail;
using namespace testing;

class BackendCassandraRetryPolicyTest : public SyncAsioContextTest {};

TEST_F(BackendCassandraRetryPolicyTest, ShouldRetryAlwaysTrue)
{
    auto retryPolicy = ExponentialBackoffRetryPolicy{ctx};
    EXPECT_TRUE(retryPolicy.shouldRetry(CassandraError{"timeout", CASS_ERROR_LIB_REQUEST_TIMED_OUT}));
    EXPECT_TRUE(retryPolicy.shouldRetry(CassandraError{"invalid data", CASS_ERROR_LIB_INVALID_DATA}));
    EXPECT_TRUE(retryPolicy.shouldRetry(CassandraError{"invalid query", CASS_ERROR_SERVER_INVALID_QUERY}));

    // this policy actually always returns true
    auto const err = CassandraError{"ok", CASS_OK};
    for (auto i = 0; i < 1024; ++i) {
        EXPECT_TRUE(retryPolicy.shouldRetry(err));
    }
}

TEST_F(BackendCassandraRetryPolicyTest, CheckComputedBackoffDelayIsCorrect)
{
    auto retryPolicy = ExponentialBackoffRetryPolicy{ctx};
    EXPECT_EQ(retryPolicy.calculateDelay(0).count(), 1);
    EXPECT_EQ(retryPolicy.calculateDelay(1).count(), 2);
    EXPECT_EQ(retryPolicy.calculateDelay(2).count(), 4);
    EXPECT_EQ(retryPolicy.calculateDelay(3).count(), 8);
    EXPECT_EQ(retryPolicy.calculateDelay(4).count(), 16);
    EXPECT_EQ(retryPolicy.calculateDelay(5).count(), 32);
    EXPECT_EQ(retryPolicy.calculateDelay(6).count(), 64);
    EXPECT_EQ(retryPolicy.calculateDelay(7).count(), 128);
    EXPECT_EQ(retryPolicy.calculateDelay(8).count(), 256);
    EXPECT_EQ(retryPolicy.calculateDelay(9).count(), 512);
    EXPECT_EQ(retryPolicy.calculateDelay(10).count(), 1024);
    EXPECT_EQ(retryPolicy.calculateDelay(11).count(),
              1024);  // 10 is max, same after that
}

TEST_F(BackendCassandraRetryPolicyTest, RetryCorrectlyExecuted)
{
    auto callCount = std::atomic_int{0};
    auto work = std::optional<boost::asio::io_context::work>{ctx};
    auto retryPolicy = ExponentialBackoffRetryPolicy{ctx};

    retryPolicy.retry([&callCount]() { ++callCount; });
    retryPolicy.retry([&callCount]() { ++callCount; });
    retryPolicy.retry([&callCount, &work]() {
        ++callCount;
        work.reset();
    });

    ctx.run();
    ASSERT_EQ(callCount, 3);
}
