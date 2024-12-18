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

#include "migration/cassandra/impl/FullTableScanner.hpp"
#include "util/LoggerFixtures.hpp"

#include <boost/asio/spawn.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <limits>

namespace {

struct TestScannerAdaper {
    TestScannerAdaper(
        testing::MockFunction<void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>& func
    )
        : callback(func) {};

    TestScannerAdaper(TestScannerAdaper const&) = default;
    TestScannerAdaper(TestScannerAdaper&&) = default;

    std::reference_wrapper<
        testing::MockFunction<void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)>>
        callback;

    void
    readByTokenRange(migration::cassandra::impl::TokenRange const& range, boost::asio::yield_context yield) const
    {
        callback.get().Call(range, yield);
    }
};
}  // namespace

struct FullTableScannerTests : public NoLoggerFixture {};

TEST_F(FullTableScannerTests, workerNumZero)
{
    testing::MockFunction<void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)> mockCallback;
    EXPECT_DEATH(
        migration::cassandra::impl::FullTableScanner<TestScannerAdaper>(
            {.ctxThreadsNum = 1, .jobsNum = 0, .cursorsPerJob = 100}, TestScannerAdaper(mockCallback)
        ),
        "jobsNum for full table scanner must be greater than 0"
    );
}

TEST_F(FullTableScannerTests, cursorsPerWorkerZero)
{
    testing::MockFunction<void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)> mockCallback;
    EXPECT_DEATH(
        migration::cassandra::impl::FullTableScanner<TestScannerAdaper>(
            {.ctxThreadsNum = 1, .jobsNum = 1, .cursorsPerJob = 0}, TestScannerAdaper(mockCallback)
        ),
        "cursorsPerJob for full table scanner must be greater than 0"
    );
}

TEST_F(FullTableScannerTests, SingleThreadCtx)
{
    testing::MockFunction<void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)> mockCallback;
    EXPECT_CALL(mockCallback, Call(testing::_, testing::_)).Times(100);
    auto scanner = migration::cassandra::impl::FullTableScanner<TestScannerAdaper>(
        {.ctxThreadsNum = 1, .jobsNum = 1, .cursorsPerJob = 100}, TestScannerAdaper(mockCallback)
    );
    scanner.wait();
}

TEST_F(FullTableScannerTests, MultipleThreadCtx)
{
    testing::MockFunction<void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)> mockCallback;
    EXPECT_CALL(mockCallback, Call(testing::_, testing::_)).Times(200);
    auto scanner = migration::cassandra::impl::FullTableScanner<TestScannerAdaper>(
        {.ctxThreadsNum = 2, .jobsNum = 2, .cursorsPerJob = 100}, TestScannerAdaper(mockCallback)
    );
    scanner.wait();
}

MATCHER(RangeMinMax, "Matches the range with min and max")
{
    return (arg.start == std::numeric_limits<std::int64_t>::min()) &&
        (arg.end == std::numeric_limits<std::int64_t>::max());
}
TEST_F(FullTableScannerTests, RangeSizeIsOne)
{
    testing::MockFunction<void(migration::cassandra::impl::TokenRange const&, boost::asio::yield_context)> mockCallback;
    EXPECT_CALL(mockCallback, Call(RangeMinMax(), testing::_)).Times(1);
    auto scanner = migration::cassandra::impl::FullTableScanner<TestScannerAdaper>(
        {.ctxThreadsNum = 2, .jobsNum = 1, .cursorsPerJob = 1}, TestScannerAdaper(mockCallback)
    );
    scanner.wait();
}
