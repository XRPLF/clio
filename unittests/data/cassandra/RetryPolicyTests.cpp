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

#include "data/cassandra/Error.hpp"
#include "data/cassandra/impl/RetryPolicy.hpp"
#include "util/Fixtures.hpp"

#include <cassandra.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace data::cassandra;
using namespace data::cassandra::impl;
using namespace testing;

struct BackendCassandraRetryPolicyTest : SyncAsioContextTest {
    ExponentialBackoffRetryPolicy retryPolicy{ctx};
};

TEST_F(BackendCassandraRetryPolicyTest, ShouldRetryAlwaysTrue)
{
    EXPECT_TRUE(retryPolicy.shouldRetry(CassandraError{"timeout", CASS_ERROR_LIB_REQUEST_TIMED_OUT}));
    EXPECT_TRUE(retryPolicy.shouldRetry(CassandraError{"invalid data", CASS_ERROR_LIB_INVALID_DATA}));
    EXPECT_TRUE(retryPolicy.shouldRetry(CassandraError{"invalid query", CASS_ERROR_SERVER_INVALID_QUERY}));

    // this policy actually always returns true
    auto const err = CassandraError{"ok", CASS_OK};
    for (auto i = 0; i < 1024; ++i) {
        EXPECT_TRUE(retryPolicy.shouldRetry(err));
    }
}

TEST_F(BackendCassandraRetryPolicyTest, RetryCorrectlyExecuted)
{
    StrictMock<MockFunction<void()>> callback;
    EXPECT_CALL(callback, Call()).Times(3);

    for (auto i = 0; i < 3; ++i) {
        retryPolicy.retry([&callback]() { callback.Call(); });
        runContext();
    }
}

TEST_F(BackendCassandraRetryPolicyTest, MutlipleRetryCancelPreviousCalls)
{
    StrictMock<MockFunction<void()>> callback;
    EXPECT_CALL(callback, Call());

    for (auto i = 0; i < 3; ++i)
        retryPolicy.retry([&callback]() { callback.Call(); });

    runContext();
}

TEST_F(BackendCassandraRetryPolicyTest, CallbackIsNotCalledIfContextDies)
{
    StrictMock<MockFunction<void()>> callback;
    retryPolicy.retry([&callback]() { callback.Call(); });
}
