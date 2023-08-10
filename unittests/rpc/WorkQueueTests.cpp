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

#include <rpc/WorkQueue.h>

#include <boost/json.hpp>

#include <mutex>
#include <semaphore>

using namespace util;

namespace {
constexpr static auto JSONConfig = R"JSON({
        "server": { "max_queue_size" : 2 },
        "workers": 4
    })JSON";
}

class RPCWorkQueueTest : public NoLoggerFixture
{
protected:
    Config cfg = Config{boost::json::parse(JSONConfig)};
};

TEST_F(RPCWorkQueueTest, WhitelistedExecutionCountAddsUp)
{
    WorkQueue queue = WorkQueue::make_WorkQueue(cfg);

    auto constexpr static TOTAL = 512u;
    uint32_t executeCount = 0u;

    std::binary_semaphore sem{0};
    std::mutex mtx;

    for (auto i = 0u; i < TOTAL; ++i)
    {
        queue.postCoro(
            [&executeCount, &sem, &mtx](auto yield) {
                std::lock_guard lk(mtx);
                if (++executeCount; executeCount == TOTAL)
                    sem.release();  // 1) note we are still in user function
            },
            true);
    }

    sem.acquire();

    // 2) so we have to allow the size of queue to decrease by one asynchronously
    std::this_thread::sleep_for(std::chrono::milliseconds{1});

    auto const report = queue.report();

    EXPECT_EQ(executeCount, TOTAL);
    EXPECT_EQ(report.at("queued"), TOTAL);
    EXPECT_EQ(report.at("current_queue_size"), 0);
    EXPECT_EQ(report.at("max_queue_size"), 2);
}

TEST_F(RPCWorkQueueTest, NonWhitelistedPreventSchedulingAtQueueLimitExceeded)
{
    auto queue = WorkQueue::make_WorkQueue(cfg);

    auto constexpr static TOTAL = 3u;
    auto expectedCount = 2u;
    auto unblocked = false;

    std::binary_semaphore sem{0};
    std::mutex mtx;
    std::condition_variable cv;

    for (auto i = 0u; i < TOTAL; ++i)
    {
        auto res = queue.postCoro(
            [&](auto yield) {
                std::unique_lock lk{mtx};
                cv.wait(lk, [&] { return unblocked == true; });

                if (--expectedCount; expectedCount == 0)
                    sem.release();
            },
            false);

        if (i == TOTAL - 1)
        {
            EXPECT_FALSE(res);

            std::unique_lock lk{mtx};
            unblocked = true;
            cv.notify_all();
        }
        else
        {
            EXPECT_TRUE(res);
        }
    }

    sem.acquire();
    EXPECT_TRUE(unblocked);
}
