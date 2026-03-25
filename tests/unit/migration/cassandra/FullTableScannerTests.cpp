#include "migration/cassandra/impl/FullTableScanner.hpp"
#include "util/MockAssert.hpp"

#include <boost/asio/spawn.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <limits>

namespace {

struct TestScannerAdapter {
    TestScannerAdapter(
        testing::MockFunction<
            void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>& func
    )
        : callback(func) {};

    TestScannerAdapter(TestScannerAdapter const&) = default;
    TestScannerAdapter(TestScannerAdapter&&) = default;

    std::reference_wrapper<testing::MockFunction<
        void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>>
        callback;

    void
    readByTokenRange(
        migration::cassandra::impl::TokenRange const& range,
        boost::asio::yield_context yield
    ) const
    {
        callback.get().Call(range, yield);
    }
};
}  // namespace

struct FullTableScannerAssertTest : common::util::WithMockAssert {};

TEST_F(FullTableScannerAssertTest, workerNumZero)
{
    testing::MockFunction<
        void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>
        mockCallback;
    EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE(
        migration::cassandra::impl::FullTableScanner<TestScannerAdapter>(
            {.ctxThreadsNum = 1, .jobsNum = 0, .cursorsPerJob = 100},
            TestScannerAdapter(mockCallback)
        ),
        ".*jobsNum for full table scanner must be greater than 0"
    );
}

TEST_F(FullTableScannerAssertTest, cursorsPerWorkerZero)
{
    testing::MockFunction<
        void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>
        mockCallback;
    EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE(
        migration::cassandra::impl::FullTableScanner<TestScannerAdapter>(
            {.ctxThreadsNum = 1, .jobsNum = 1, .cursorsPerJob = 0}, TestScannerAdapter(mockCallback)
        ),
        ".*cursorsPerJob for full table scanner must be greater than 0"
    );
}

struct FullTableScannerTests : public virtual ::testing::Test {};

TEST_F(FullTableScannerTests, SingleThreadCtx)
{
    testing::MockFunction<
        void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>
        mockCallback;
    EXPECT_CALL(mockCallback, Call(testing::_, testing::_)).Times(100);
    auto scanner = migration::cassandra::impl::FullTableScanner<TestScannerAdapter>(
        {.ctxThreadsNum = 1, .jobsNum = 1, .cursorsPerJob = 100}, TestScannerAdapter(mockCallback)
    );
    scanner.wait();
}

TEST_F(FullTableScannerTests, MultipleThreadCtx)
{
    testing::MockFunction<
        void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>
        mockCallback;
    EXPECT_CALL(mockCallback, Call(testing::_, testing::_)).Times(200);
    auto scanner = migration::cassandra::impl::FullTableScanner<TestScannerAdapter>(
        {.ctxThreadsNum = 2, .jobsNum = 2, .cursorsPerJob = 100}, TestScannerAdapter(mockCallback)
    );
    scanner.wait();
}

MATCHER(rangeMinMax, "Matches the range with min and max")
{
    return (arg.start == std::numeric_limits<std::int64_t>::min()) &&
        (arg.end == std::numeric_limits<std::int64_t>::max());
}
TEST_F(FullTableScannerTests, RangeSizeIsOne)
{
    testing::MockFunction<
        void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>
        mockCallback;
    EXPECT_CALL(mockCallback, Call(rangeMinMax(), testing::_)).Times(1);
    auto scanner = migration::cassandra::impl::FullTableScanner<TestScannerAdapter>(
        {.ctxThreadsNum = 2, .jobsNum = 1, .cursorsPerJob = 1}, TestScannerAdapter(mockCallback)
    );
    scanner.wait();
}
