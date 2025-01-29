//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "util/StrandedPriorityQueue.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/context/BasicExecutionContext.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace util;

namespace {

struct TestData {
    uint32_t seq;

    auto
    operator<=>(TestData const&) const = default;
};

}  // namespace

TEST(StrandedPriorityQueueTests, DefaultPriority)
{
    util::async::CoroExecutionContext ctx;
    StrandedPriorityQueue<TestData> queue{ctx.makeStrand()};

    for (auto i = 0u; i < 100u; ++i) {
        EXPECT_TRUE(queue.enqueue(TestData{.seq = i}));
    }

    EXPECT_FALSE(queue.empty());

    auto next = 99u;
    while (auto maybeValue = queue.dequeue()) {
        EXPECT_EQ(maybeValue->seq, next--);
    }

    EXPECT_TRUE(queue.empty());
}

TEST(StrandedPriorityQueueTests, CustomPriority)
{
    struct Comp {
        [[nodiscard]] bool
        operator()(TestData const& lhs, TestData const& rhs) const noexcept
        {
            return lhs.seq > rhs.seq;
        }
    };

    util::async::CoroExecutionContext ctx;
    StrandedPriorityQueue<TestData, Comp> queue{ctx.makeStrand()};

    for (auto i = 0u; i < 100u; ++i) {
        EXPECT_TRUE(queue.enqueue(TestData{.seq = i}));
    }

    EXPECT_FALSE(queue.empty());

    auto next = 0u;
    while (auto maybeValue = queue.dequeue()) {
        EXPECT_EQ(maybeValue->seq, next++);
    }

    EXPECT_TRUE(queue.empty());
}

TEST(StrandedPriorityQueueTests, MultipleThreadsUnlimitedQueue)
{
    async::CoroExecutionContext realCtx{6};
    async::AnyExecutionContext ctx{realCtx};
    StrandedPriorityQueue<TestData> queue{ctx.makeStrand()};

    EXPECT_TRUE(queue.empty());
    static constexpr auto kTOTAL_THREADS = 5u;
    static constexpr auto kTOTAL_ITEMS_PER_THREAD = 100u;

    std::atomic_size_t totalEnqueued = 0uz;
    std::vector<async::AnyOperation<void>> tasks;
    tasks.reserve(kTOTAL_THREADS);

    for (auto batchIdx = 0u; batchIdx < kTOTAL_THREADS; ++batchIdx) {
        // enqueue batches tasks running on multiple threads
        tasks.push_back(ctx.execute([&queue, batchIdx, &totalEnqueued] {
            for (auto i = 0u; i < kTOTAL_ITEMS_PER_THREAD; ++i) {
                if (queue.enqueue(TestData{.seq = (batchIdx * kTOTAL_ITEMS_PER_THREAD) + i}))
                    ++totalEnqueued;
            }
        }));
    }

    for (auto& task : tasks)
        task.wait();

    auto next = (kTOTAL_ITEMS_PER_THREAD * kTOTAL_THREADS) - 1;
    while (auto maybeValue = queue.dequeue()) {
        EXPECT_EQ(maybeValue->seq, next--);
    }

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(totalEnqueued, kTOTAL_ITEMS_PER_THREAD * kTOTAL_THREADS);
}

TEST(StrandedPriorityQueueTests, MultipleThreadsLimitedQueue)
{
    static constexpr auto kQUEUE_SIZE_LIMIT = 32uz;
    static constexpr auto kTOTAL_THREADS = 5u;
    static constexpr auto kTOTAL_ITEMS_PER_THREAD = 100u;

    async::CoroExecutionContext realCtx{8};
    async::AnyExecutionContext ctx{realCtx};
    StrandedPriorityQueue<TestData> queue{ctx.makeStrand(), kQUEUE_SIZE_LIMIT};

    EXPECT_TRUE(queue.empty());

    std::atomic_size_t totalEnqueued = 0uz;
    std::atomic_size_t totalSleepCycles = 0uz;
    std::vector<async::AnyOperation<void>> tasks;
    tasks.reserve(kTOTAL_THREADS);

    std::unordered_set<uint32_t> expectedSequences;

    for (auto batchIdx = 0u; batchIdx < kTOTAL_THREADS; ++batchIdx) {
        for (auto i = 0u; i < kTOTAL_ITEMS_PER_THREAD; ++i) {
            expectedSequences.insert((batchIdx * kTOTAL_ITEMS_PER_THREAD) + i);
        }

        // enqueue batches tasks running on multiple threads
        tasks.push_back(ctx.execute([&queue, batchIdx, &totalEnqueued, &totalSleepCycles] {
            for (auto i = 0u; i < kTOTAL_ITEMS_PER_THREAD; ++i) {
                auto data = TestData{.seq = (batchIdx * kTOTAL_ITEMS_PER_THREAD) + i};
                while (not queue.enqueue(data)) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds{1});
                    ++totalSleepCycles;
                }
                ++totalEnqueued;
            }
        }));
    }

    EXPECT_FALSE(expectedSequences.empty());

    auto loader = ctx.execute([&queue, &expectedSequences] {
        while (not expectedSequences.empty()) {
            while (auto maybeValue = queue.dequeue()) {
                EXPECT_TRUE(expectedSequences.contains(maybeValue->seq));
                expectedSequences.erase(maybeValue->seq);
            }
        }
    });

    for (auto& task : tasks)
        task.wait();
    loader.wait();

    EXPECT_TRUE(queue.empty());
    EXPECT_TRUE(expectedSequences.empty());
    EXPECT_EQ(totalEnqueued, kTOTAL_ITEMS_PER_THREAD * kTOTAL_THREADS);
    EXPECT_GE(totalSleepCycles, 1uz);
}

TEST(StrandedPriorityQueueTests, ReturnsNulloptIfQueueEmpty)
{
    async::CoroExecutionContext realCtx;
    StrandedPriorityQueue<TestData> queue{realCtx.makeStrand()};

    EXPECT_TRUE(queue.empty());
    auto maybeValue = queue.dequeue();
    EXPECT_FALSE(maybeValue.has_value());
}
