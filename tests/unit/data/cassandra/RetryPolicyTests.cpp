#include "data/cassandra/Error.hpp"
#include "data/cassandra/impl/RetryPolicy.hpp"
#include "util/AsioContextTestFixture.hpp"

#include <cassandra.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace data::cassandra;
using namespace data::cassandra::impl;
using namespace testing;

struct BackendCassandraRetryPolicyTest : SyncAsioContextTest {
    ExponentialBackoffRetryPolicy retryPolicy{ctx_};
};

TEST_F(BackendCassandraRetryPolicyTest, ShouldRetryAlwaysTrue)
{
    EXPECT_TRUE(
        retryPolicy.shouldRetry(CassandraError{"timeout", CASS_ERROR_LIB_REQUEST_TIMED_OUT})
    );
    EXPECT_TRUE(
        retryPolicy.shouldRetry(CassandraError{"invalid data", CASS_ERROR_LIB_INVALID_DATA})
    );
    EXPECT_TRUE(
        retryPolicy.shouldRetry(CassandraError{"invalid query", CASS_ERROR_SERVER_INVALID_QUERY})
    );

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

TEST_F(BackendCassandraRetryPolicyTest, MultipleRetryCancelPreviousCalls)
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
