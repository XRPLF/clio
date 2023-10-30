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

#include <data/BackendCounters.h>

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <gtest/gtest.h>

using namespace data;

class BackendCountersTest : public ::testing::Test {
protected:
    static boost::json::object
    emptyReport()
    {
        return boost::json::parse(R"({
            "too_busy": 0,
            "write_sync": 0,
            "write_sync_retry": 0,
            "write_async_pending": 0,
            "write_async_completed": 0,
            "write_async_retry": 0,
            "write_async_error": 0,
            "read_async_pending": 0,
            "read_async_completed": 0,
            "read_async_retry": 0,
            "read_async_error": 0
        })")
            .as_object();
    }
};

TEST_F(BackendCountersTest, EmptyByDefault)
{
    auto const counters = BackendCounters::make();
    EXPECT_EQ(counters->report(), emptyReport());
}

TEST_F(BackendCountersTest, RegisterTooBusy)
{
    auto const counters = BackendCounters::make();
    counters->registerTooBusy();
    counters->registerTooBusy();
    counters->registerTooBusy();
    auto expectedReport = emptyReport();
    expectedReport["too_busy"] = 3;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterWriteSync)
{
    auto const counters = BackendCounters::make();
    counters->registerWriteSync();
    counters->registerWriteSync();
    auto expectedReport = emptyReport();
    expectedReport["write_sync"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterWriteSyncRetry)
{
    auto const counters = BackendCounters::make();
    counters->registerWriteSyncRetry();
    counters->registerWriteSyncRetry();
    counters->registerWriteSyncRetry();
    auto expectedReport = emptyReport();
    expectedReport["write_sync_retry"] = 3;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterWriteStarted)
{
    auto const counters = BackendCounters::make();
    counters->registerWriteStarted();
    counters->registerWriteStarted();
    auto expectedReport = emptyReport();
    expectedReport["write_async_pending"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterWriteFinished)
{
    auto const counters = BackendCounters::make();
    counters->registerWriteStarted();
    counters->registerWriteStarted();
    counters->registerWriteStarted();
    counters->registerWriteFinished();
    counters->registerWriteFinished();
    auto expectedReport = emptyReport();
    expectedReport["write_async_pending"] = 1;
    expectedReport["write_async_completed"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterWriteRetry)
{
    auto const counters = BackendCounters::make();
    counters->registerWriteRetry();
    counters->registerWriteRetry();
    auto expectedReport = emptyReport();
    expectedReport["write_async_retry"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterReadStarted)
{
    auto const counters = BackendCounters::make();
    counters->registerReadStarted();
    counters->registerReadStarted();
    auto expectedReport = emptyReport();
    expectedReport["read_async_pending"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterReadFinished)
{
    auto const counters = BackendCounters::make();
    counters->registerReadStarted();
    counters->registerReadStarted();
    counters->registerReadStarted();
    counters->registerReadFinished();
    counters->registerReadFinished();
    auto expectedReport = emptyReport();
    expectedReport["read_async_pending"] = 1;
    expectedReport["read_async_completed"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterReadStartedFinishedWithCounters)
{
    static constexpr auto OPERATIONS_STARTED = 7u;
    static constexpr auto OPERATIONS_COMPLETED = 4u;
    auto const counters = BackendCounters::make();
    counters->registerReadStarted(OPERATIONS_STARTED);
    counters->registerReadFinished(OPERATIONS_COMPLETED);
    auto expectedReport = emptyReport();
    expectedReport["read_async_pending"] = OPERATIONS_STARTED - OPERATIONS_COMPLETED;
    expectedReport["read_async_completed"] = OPERATIONS_COMPLETED;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterReadRetry)
{
    auto const counters = BackendCounters::make();
    counters->registerReadRetry();
    counters->registerReadRetry();
    auto expectedReport = emptyReport();
    expectedReport["read_async_retry"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterReadError)
{
    static constexpr auto OPERATIONS_STARTED = 7u;
    static constexpr auto OPERATIONS_ERROR = 2u;
    static constexpr auto OPERATIONS_COMPLETED = 1u;
    auto const counters = BackendCounters::make();
    counters->registerReadStarted(OPERATIONS_STARTED);
    counters->registerReadError(OPERATIONS_ERROR);
    counters->registerReadFinished(OPERATIONS_COMPLETED);
    auto expectedReport = emptyReport();
    expectedReport["read_async_pending"] = OPERATIONS_STARTED - OPERATIONS_COMPLETED - OPERATIONS_ERROR;
    expectedReport["read_async_completed"] = OPERATIONS_COMPLETED;
    expectedReport["read_async_error"] = OPERATIONS_ERROR;
    EXPECT_EQ(counters->report(), expectedReport);
}
