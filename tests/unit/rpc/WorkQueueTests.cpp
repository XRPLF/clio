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

#include "rpc/WorkQueue.hpp"
#include "util/MockPrometheus.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <semaphore>
#include <vector>

using namespace util;
using namespace util::config;
using namespace rpc;
using namespace util::prometheus;

struct RPCWorkQueueTestBase : public virtual ::testing::Test {
    ClioConfigDefinition cfg;
    WorkQueue queue;

    RPCWorkQueueTestBase(uint32_t workers, uint32_t maxQueueSize)
        : cfg{
              {"server.max_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(maxQueueSize)},
              {"workers", ConfigValue{ConfigType::Integer}.defaultValue(workers)},
          }
        , queue{WorkQueue::makeWorkQueue(cfg)}
    {
    }
};

struct WorkQueueTest : WithPrometheus, RPCWorkQueueTestBase {
    WorkQueueTest() : RPCWorkQueueTestBase(/* workers = */ 4, /* maxQueueSize = */ 2)
    {
    }
};

TEST_F(WorkQueueTest, WhitelistedExecutionCountAddsUp)
{
    static constexpr auto kTOTAL = 512u;
    std::atomic_uint32_t executeCount = 0u;

    for (auto i = 0u; i < kTOTAL; ++i) {
        queue.postCoro([&executeCount](auto /* yield */) { ++executeCount; }, /* isWhiteListed = */ true);
    }

    queue.stop();

    auto const report = queue.report();

    EXPECT_EQ(executeCount, kTOTAL);
    EXPECT_EQ(report.at("queued"), kTOTAL);
    EXPECT_EQ(report.at("current_queue_size"), 0);
    EXPECT_EQ(report.at("max_queue_size"), 2);
}

TEST_F(WorkQueueTest, NonWhitelistedPreventSchedulingAtQueueLimitExceeded)
{
    static constexpr auto kTOTAL = 3u;
    auto unblocked = false;

    std::mutex mtx;
    std::condition_variable cv;

    for (auto i = 0u; i < kTOTAL; ++i) {
        auto res = queue.postCoro(
            [&](auto /* yield */) {
                std::unique_lock lk{mtx};
                cv.wait(lk, [&] { return unblocked; });
            },
            /* isWhiteListed = */ false
        );

        if (i == kTOTAL - 1) {
            EXPECT_FALSE(res);

            std::unique_lock const lk{mtx};
            unblocked = true;
            cv.notify_all();
        } else {
            EXPECT_TRUE(res);
        }
    }

    queue.stop();
    EXPECT_TRUE(unblocked);
}

struct WorkQueuePriorityTest : WithPrometheus, virtual ::testing::Test {
    WorkQueue queue{WorkQueue::kDONT_START_PROCESSING_TAG, /* numWorkers = */ 1, /* maxSize = */ 100};
};

TEST_F(WorkQueuePriorityTest, HighPriorityTasks)
{
    static constexpr auto kTOTAL = 10;
    std::vector<WorkQueue::Priority> executionOrder;
    std::mutex mtx;

    for (int i = 0; i < kTOTAL; ++i) {
        queue.postCoro(
            [&](auto) {
                std::lock_guard const lock(mtx);
                executionOrder.push_back(WorkQueue::Priority::High);
            },
            /* isWhiteListed = */ true,
            WorkQueue::Priority::High
        );
        queue.postCoro(
            [&](auto) {
                std::lock_guard const lock(mtx);
                executionOrder.push_back(WorkQueue::Priority::Default);
            },
            /* isWhiteListed = */ true,
            WorkQueue::Priority::Default
        );
    }

    queue.startProcessing();
    queue.stop();

    // with 1 worker and the above, the execution order is deterministic
    // we should see 4 high prio tasks, then 1 normal prio task, until high prio tasks are depleted
    std::vector<WorkQueue::Priority> const expectedOrder = {
        WorkQueue::Priority::High,    WorkQueue::Priority::High,    WorkQueue::Priority::High,
        WorkQueue::Priority::High,    WorkQueue::Priority::Default, WorkQueue::Priority::High,
        WorkQueue::Priority::High,    WorkQueue::Priority::High,    WorkQueue::Priority::High,
        WorkQueue::Priority::Default, WorkQueue::Priority::High,    WorkQueue::Priority::High,
        WorkQueue::Priority::Default, WorkQueue::Priority::Default, WorkQueue::Priority::Default,
        WorkQueue::Priority::Default, WorkQueue::Priority::Default, WorkQueue::Priority::Default,
        WorkQueue::Priority::Default, WorkQueue::Priority::Default,
    };

    ASSERT_EQ(executionOrder.size(), expectedOrder.size());
    for (auto i = 0uz; i < executionOrder.size(); ++i) {
        EXPECT_EQ(executionOrder[i], expectedOrder[i]) << "Mismatch at index " << i;
    }
}

struct WorkQueueStopTest : WorkQueueTest {
    testing::StrictMock<testing::MockFunction<void()>> onTasksComplete;
    testing::StrictMock<testing::MockFunction<void()>> taskMock;
};

TEST_F(WorkQueueStopTest, RejectsNewTasksWhenStopping)
{
    EXPECT_CALL(taskMock, Call());
    EXPECT_TRUE(queue.postCoro([this](auto /* yield */) { taskMock.Call(); }, /* isWhiteListed = */ false));

    queue.requestStop();
    EXPECT_FALSE(queue.postCoro([this](auto /* yield */) { taskMock.Call(); }, /* isWhiteListed = */ false));

    queue.stop();
}

TEST_F(WorkQueueStopTest, CallsOnTasksCompleteWhenStoppingAndQueueIsEmpty)
{
    EXPECT_CALL(taskMock, Call());
    EXPECT_TRUE(queue.postCoro([this](auto /* yield */) { taskMock.Call(); }, /* isWhiteListed = */ false));

    EXPECT_CALL(onTasksComplete, Call()).WillOnce([&]() { EXPECT_EQ(queue.size(), 0u); });
    queue.requestStop(onTasksComplete.AsStdFunction());
    queue.stop();
}

TEST_F(WorkQueueStopTest, CallsOnTasksCompleteWhenStoppingOnLastTask)
{
    std::binary_semaphore semaphore{0};

    EXPECT_CALL(taskMock, Call());
    EXPECT_TRUE(queue.postCoro(
        [&](auto /* yield */) {
            taskMock.Call();
            semaphore.acquire();
        },
        /* isWhiteListed = */ false
    ));

    EXPECT_CALL(onTasksComplete, Call()).WillOnce([&]() { EXPECT_EQ(queue.size(), 0u); });
    queue.requestStop(onTasksComplete.AsStdFunction());
    semaphore.release();

    queue.stop();
}

struct WorkQueueMockPrometheusTest : WithMockPrometheus, RPCWorkQueueTestBase {
    WorkQueueMockPrometheusTest() : RPCWorkQueueTestBase(/* workers = */ 1, /*maxQueueSize = */ 2)
    {
    }
};

TEST_F(WorkQueueMockPrometheusTest, postCoroCounters)
{
    auto& queuedMock = makeMock<CounterInt>("work_queue_queued_total_number", "");
    auto& durationMock = makeMock<CounterInt>("work_queue_cumulative_tasks_duration_us", "");
    auto& curSizeMock = makeMock<GaugeInt>("work_queue_current_size", "");

    std::binary_semaphore semaphore{0};

    EXPECT_CALL(curSizeMock, value()).WillOnce(::testing::Return(0)).WillRepeatedly(::testing::Return(1));
    EXPECT_CALL(curSizeMock, add(1));
    EXPECT_CALL(queuedMock, add(1));
    EXPECT_CALL(durationMock, add(::testing::Ge(0))).WillOnce([&](auto) {
        EXPECT_CALL(curSizeMock, add(-1));
        EXPECT_CALL(curSizeMock, value()).WillOnce(::testing::Return(0));
        semaphore.release();
    });

    auto const res = queue.postCoro([&](auto /* yield */) { semaphore.acquire(); }, /* isWhiteListed = */ false);

    ASSERT_TRUE(res);
    queue.stop();
}
