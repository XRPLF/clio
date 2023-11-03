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

#include <util/Fixtures.h>
#include <util/MockPrometheus.h>

#include <rpc/WorkQueue.h>

#include <boost/json.hpp>

#include <mutex>
#include <semaphore>

using namespace util;
using namespace rpc;
using namespace util::prometheus;

namespace {
constexpr auto JSONConfig = R"JSON({
        "server": { "max_queue_size" : 2 },
        "workers": 4
    })JSON";
}  // namespace

struct RPCWorkQueueTestBase : NoLoggerFixture {
    Config cfg = Config{boost::json::parse(JSONConfig)};
    WorkQueue queue = WorkQueue::make_WorkQueue(cfg);
};

struct RPCWorkQueueTest : WithPrometheus, RPCWorkQueueTestBase {};

TEST_F(RPCWorkQueueTest, WhitelistedExecutionCountAddsUp)
{
    auto constexpr static TOTAL = 512u;
    uint32_t executeCount = 0u;

    std::mutex mtx;

    for (auto i = 0u; i < TOTAL; ++i) {
        queue.postCoro(
            [&executeCount, &mtx](auto /* yield */) {
                std::lock_guard const lk(mtx);
                ++executeCount;
            },
            true
        );
    }

    queue.join();

    auto const report = queue.report();

    EXPECT_EQ(executeCount, TOTAL);
    EXPECT_EQ(report.at("queued"), TOTAL);
    EXPECT_EQ(report.at("current_queue_size"), 0);
    EXPECT_EQ(report.at("max_queue_size"), 2);
}

TEST_F(RPCWorkQueueTest, NonWhitelistedPreventSchedulingAtQueueLimitExceeded)
{
    auto constexpr static TOTAL = 3u;
    auto expectedCount = 2u;
    auto unblocked = false;

    std::mutex mtx;
    std::condition_variable cv;

    for (auto i = 0u; i < TOTAL; ++i) {
        auto res = queue.postCoro(
            [&](auto /* yield */) {
                std::unique_lock lk{mtx};
                cv.wait(lk, [&] { return unblocked; });

                --expectedCount;
            },
            false
        );

        if (i == TOTAL - 1) {
            EXPECT_FALSE(res);

            std::unique_lock const lk{mtx};
            unblocked = true;
            cv.notify_all();
        } else {
            EXPECT_TRUE(res);
        }
    }

    queue.join();
    EXPECT_TRUE(unblocked);
}

struct RPCWorkQueueMockPrometheusTest : WithMockPrometheus, RPCWorkQueueTestBase {};

TEST_F(RPCWorkQueueMockPrometheusTest, postCoroCouhters)
{
    auto& queuedMock = makeMock<CounterInt>("work_queue_queued_total_number", "");
    auto& durationMock = makeMock<CounterInt>("work_queue_cumulitive_tasks_duration_us", "");
    auto& curSizeMock = makeMock<GaugeInt>("work_queue_current_size", "");

    std::mutex mtx;
    bool canContinue = false;
    std::condition_variable cv;

    EXPECT_CALL(curSizeMock, value()).WillOnce(::testing::Return(0));
    EXPECT_CALL(curSizeMock, add(1));
    EXPECT_CALL(queuedMock, add(1));
    EXPECT_CALL(durationMock, add(::testing::Gt(0))).WillOnce([&](auto) {
        EXPECT_CALL(curSizeMock, add(-1));
        std::unique_lock const lk{mtx};
        canContinue = true;
        cv.notify_all();
    });

    auto const res = queue.postCoro(
        [&](auto /* yield */) {
            std::unique_lock lk{mtx};
            cv.wait(lk, [&]() { return canContinue; });
        },
        false
    );

    ASSERT_TRUE(res);
    queue.join();
}
