#include "migration/cassandra/impl/FullTableScanner.hpp"
#include "util/MockAssert.hpp"

#include <boost/asio/spawn.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

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

struct FailingAndSlowAdapter {
    std::reference_wrapper<std::atomic_uint> calls;
    std::reference_wrapper<std::atomic_bool> slowWorkerFinished;

    void
    readByTokenRange(
        migration::cassandra::impl::TokenRange const&,
        boost::asio::yield_context
    ) const
    {
        if (calls.get().fetch_add(1) == 0u)
            throw std::runtime_error("scan failure");

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        slowWorkerFinished.get() = true;
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

TEST_F(FullTableScannerAssertTest, contextThreadsZero)
{
    testing::MockFunction<
        void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>
        mockCallback;
    EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE(
        migration::cassandra::impl::FullTableScanner<TestScannerAdapter>(
            {.ctxThreadsNum = 0, .jobsNum = 1, .cursorsPerJob = 1}, TestScannerAdapter(mockCallback)
        ),
        ".*ctxThreadsNum for full table scanner must be greater than 0"
    );
}

TEST_F(FullTableScannerAssertTest, cursorsNumOverflow)
{
    testing::MockFunction<
        void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>
        mockCallback;
    EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE(
        migration::cassandra::impl::FullTableScanner<TestScannerAdapter>(
            {.ctxThreadsNum = 1,
             .jobsNum = std::numeric_limits<std::uint32_t>::max(),
             .cursorsPerJob = std::numeric_limits<std::uint32_t>::max()},
            TestScannerAdapter(mockCallback)
        ),
        ".*jobsNum \\* cursorsPerJob for full table scanner must fit in uint32_t"
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
    scanner.waitForAllAndThrowOnError();
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
    scanner.waitForAllAndThrowOnError();
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
    scanner.waitForAllAndThrowOnError();
}

TEST_F(FullTableScannerTests, WaitPropagatesWorkerError)
{
    testing::MockFunction<
        void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>
        mockCallback;
    EXPECT_CALL(mockCallback, Call(testing::_, testing::_))
        .WillRepeatedly(testing::Throw(std::runtime_error("scan failure")));
    auto scanner = migration::cassandra::impl::FullTableScanner<TestScannerAdapter>(
        {.ctxThreadsNum = 1, .jobsNum = 1, .cursorsPerJob = 1}, TestScannerAdapter(mockCallback)
    );
    try {
        scanner.waitForAllAndThrowOnError();
        FAIL() << "expected waitForAllAndThrowOnError() to throw";
    } catch (std::runtime_error const& e) {
        EXPECT_THAT(std::string{e.what()}, testing::HasSubstr("scan failure"));
    }
}

TEST_F(FullTableScannerTests, WaitReportsPartialFailure)
{
    // Two ranges across two workers; exactly one read fails. The wait must still throw and report
    // the failed-worker count.
    testing::MockFunction<
        void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>
        mockCallback;
    EXPECT_CALL(mockCallback, Call(testing::_, testing::_))
        .WillOnce(testing::Throw(std::runtime_error("scan failure")))
        .WillRepeatedly(testing::Return());
    auto scanner = migration::cassandra::impl::FullTableScanner<TestScannerAdapter>(
        {.ctxThreadsNum = 2, .jobsNum = 2, .cursorsPerJob = 1}, TestScannerAdapter(mockCallback)
    );
    try {
        scanner.waitForAllAndThrowOnError();
        FAIL() << "expected waitForAllAndThrowOnError() to throw";
    } catch (std::runtime_error const& e) {
        EXPECT_THAT(std::string{e.what()}, testing::HasSubstr("1 of 2 workers"));
    }
}

TEST_F(FullTableScannerTests, WaitJoinsAllWorkersBeforeThrowing)
{
    std::atomic_uint calls = 0;
    std::atomic_bool slowWorkerFinished = false;

    auto scanner = migration::cassandra::impl::FullTableScanner<FailingAndSlowAdapter>(
        {.ctxThreadsNum = 2, .jobsNum = 2, .cursorsPerJob = 1},
        FailingAndSlowAdapter{.calls = calls, .slowWorkerFinished = slowWorkerFinished}
    );

    EXPECT_THROW(scanner.waitForAllAndThrowOnError(), std::runtime_error);
    EXPECT_TRUE(slowWorkerFinished);
}
