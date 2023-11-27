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

#include "data/BackendCounters.h"
#include "util/MockPrometheus.h"
#include "util/prometheus/Counter.h"
#include "util/prometheus/Gauge.h"
#include "util/prometheus/Histogram.h"
#include <chrono>
#include <gmock/gmock.h>

#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

using namespace data;
using namespace util::prometheus;

struct BackendCountersTest : WithPrometheus {
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

    BackendCounters::PtrType const counters = BackendCounters::make();
    std::chrono::steady_clock::time_point startTime{};
};

TEST_F(BackendCountersTest, EmptyByDefault)
{
    EXPECT_EQ(counters->report(), emptyReport());
}

TEST_F(BackendCountersTest, RegisterTooBusy)
{
    counters->registerTooBusy();
    counters->registerTooBusy();
    counters->registerTooBusy();

    auto expectedReport = emptyReport();
    expectedReport["too_busy"] = 3;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterWriteSync)
{
    std::chrono::steady_clock::time_point const startTime{};
    counters->registerWriteSync(startTime);
    counters->registerWriteSync(startTime);

    auto expectedReport = emptyReport();
    expectedReport["write_sync"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterWriteSyncRetry)
{
    counters->registerWriteSyncRetry();
    counters->registerWriteSyncRetry();
    counters->registerWriteSyncRetry();

    auto expectedReport = emptyReport();
    expectedReport["write_sync_retry"] = 3;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterWriteStarted)
{
    counters->registerWriteStarted();
    counters->registerWriteStarted();

    auto expectedReport = emptyReport();
    expectedReport["write_async_pending"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterWriteFinished)
{
    counters->registerWriteStarted();
    counters->registerWriteStarted();
    counters->registerWriteStarted();
    counters->registerWriteFinished(startTime);
    counters->registerWriteFinished(startTime);

    auto expectedReport = emptyReport();
    expectedReport["write_async_pending"] = 1;
    expectedReport["write_async_completed"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterWriteRetry)
{
    counters->registerWriteRetry();
    counters->registerWriteRetry();

    auto expectedReport = emptyReport();
    expectedReport["write_async_retry"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterReadStarted)
{
    counters->registerReadStarted();
    counters->registerReadStarted();

    auto expectedReport = emptyReport();
    expectedReport["read_async_pending"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterReadFinished)
{
    counters->registerReadStarted();
    counters->registerReadStarted();
    counters->registerReadStarted();
    counters->registerReadFinished(startTime);
    counters->registerReadFinished(startTime);

    auto expectedReport = emptyReport();
    expectedReport["read_async_pending"] = 1;
    expectedReport["read_async_completed"] = 2;
    EXPECT_EQ(counters->report(), expectedReport);
}

TEST_F(BackendCountersTest, RegisterReadStartedFinishedWithCounters)
{
    static constexpr auto OPERATIONS_STARTED = 7u;
    static constexpr auto OPERATIONS_COMPLETED = 4u;

    counters->registerReadStarted(OPERATIONS_STARTED);
    counters->registerReadFinished(startTime, OPERATIONS_COMPLETED);

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

    counters->registerReadStarted(OPERATIONS_STARTED);
    counters->registerReadError(OPERATIONS_ERROR);
    counters->registerReadFinished(startTime, OPERATIONS_COMPLETED);

    auto expectedReport = emptyReport();
    expectedReport["read_async_pending"] = OPERATIONS_STARTED - OPERATIONS_COMPLETED - OPERATIONS_ERROR;
    expectedReport["read_async_completed"] = OPERATIONS_COMPLETED;
    expectedReport["read_async_error"] = OPERATIONS_ERROR;
    EXPECT_EQ(counters->report(), expectedReport);
}

struct BackendCountersMockPrometheusTest : WithMockPrometheus {
    BackendCounters::PtrType const counters = BackendCounters::make();
};

TEST_F(BackendCountersMockPrometheusTest, registerTooBusy)
{
    auto& counter = makeMock<CounterInt>("backend_too_busy_total_number", "");
    EXPECT_CALL(counter, add(1));
    counters->registerTooBusy();
}

TEST_F(BackendCountersMockPrometheusTest, registerWriteSync)
{
    auto& counter = makeMock<CounterInt>("backend_operations_total_number", "{operation=\"write_sync\"}");
    auto& histogram = makeMock<HistogramInt>("backend_duration_milliseconds_histogram", "{operation=\"write\"}");
    EXPECT_CALL(counter, add(1));
    EXPECT_CALL(histogram, observe(testing::_));
    std::chrono::steady_clock::time_point const startTime{};
    counters->registerWriteSync(startTime);
}

TEST_F(BackendCountersMockPrometheusTest, registerWriteSyncRetry)
{
    auto& counter = makeMock<CounterInt>("backend_operations_total_number", "{operation=\"write_sync_retry\"}");
    EXPECT_CALL(counter, add(1));
    counters->registerWriteSyncRetry();
}

TEST_F(BackendCountersMockPrometheusTest, registerWriteStarted)
{
    auto& counter =
        makeMock<GaugeInt>("backend_operations_current_number", "{operation=\"write_async\",status=\"pending\"}");
    EXPECT_CALL(counter, add(1));
    counters->registerWriteStarted();
}

TEST_F(BackendCountersMockPrometheusTest, registerWriteFinished)
{
    auto& pendingCounter =
        makeMock<GaugeInt>("backend_operations_current_number", "{operation=\"write_async\",status=\"pending\"}");
    auto& completedCounter =
        makeMock<CounterInt>("backend_operations_total_number", "{operation=\"write_async\",status=\"completed\"}");
    auto& histogram = makeMock<HistogramInt>("backend_duration_milliseconds_histogram", "{operation=\"write\"}");
    EXPECT_CALL(pendingCounter, value()).WillOnce(testing::Return(1));
    EXPECT_CALL(pendingCounter, add(-1));
    EXPECT_CALL(completedCounter, add(1));
    EXPECT_CALL(histogram, observe(testing::_));
    std::chrono::steady_clock::time_point const startTime{};
    counters->registerWriteFinished(startTime);
}

TEST_F(BackendCountersMockPrometheusTest, registerWriteRetry)
{
    auto& counter =
        makeMock<CounterInt>("backend_operations_total_number", "{operation=\"write_async\",status=\"retry\"}");
    EXPECT_CALL(counter, add(1));
    counters->registerWriteRetry();
}

TEST_F(BackendCountersMockPrometheusTest, registerReadStarted)
{
    auto& counter =
        makeMock<GaugeInt>("backend_operations_current_number", "{operation=\"read_async\",status=\"pending\"}");
    EXPECT_CALL(counter, add(1));
    counters->registerReadStarted();
}

TEST_F(BackendCountersMockPrometheusTest, registerReadFinished)
{
    auto& pendingCounter =
        makeMock<GaugeInt>("backend_operations_current_number", "{operation=\"read_async\",status=\"pending\"}");
    auto& completedCounter =
        makeMock<CounterInt>("backend_operations_total_number", "{operation=\"read_async\",status=\"completed\"}");
    auto& histogram = makeMock<HistogramInt>("backend_duration_milliseconds_histogram", "{operation=\"read\"}");
    EXPECT_CALL(pendingCounter, value()).WillOnce(testing::Return(2));
    EXPECT_CALL(pendingCounter, add(-2));
    EXPECT_CALL(completedCounter, add(2));
    EXPECT_CALL(histogram, observe(testing::_)).Times(2);
    std::chrono::steady_clock::time_point const startTime{};
    counters->registerReadFinished(startTime, 2);
}

TEST_F(BackendCountersMockPrometheusTest, registerReadRetry)
{
    auto& counter =
        makeMock<CounterInt>("backend_operations_total_number", "{operation=\"read_async\",status=\"retry\"}");
    EXPECT_CALL(counter, add(1));
    counters->registerReadRetry();
}

TEST_F(BackendCountersMockPrometheusTest, registerReadError)
{
    auto& pendingCounter =
        makeMock<GaugeInt>("backend_operations_current_number", "{operation=\"read_async\",status=\"pending\"}");
    auto& errorCounter =
        makeMock<CounterInt>("backend_operations_total_number", "{operation=\"read_async\",status=\"error\"}");
    EXPECT_CALL(pendingCounter, value()).WillOnce(testing::Return(1));
    EXPECT_CALL(pendingCounter, add(-1));
    EXPECT_CALL(errorCounter, add(1));
    counters->registerReadError();
}
