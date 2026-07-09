#include "util/Batching.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <mutex>
#include <thread>
#include <vector>

TEST(BatchingTests, simpleBatch)
{
    std::vector<int> const input{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> output;

    util::forEachBatch(input, 3, [&](auto begin, auto end) {
        std::copy(begin, end, std::back_inserter(output));
        EXPECT_LE(std::distance(begin, end), 3);
    });

    EXPECT_EQ(input, output);
}

TEST(BatchingTests, simpleBatchEven)
{
    std::vector<int> const input{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> output;

    util::forEachBatch(input, 2, [&](auto begin, auto end) {
        std::copy(begin, end, std::back_inserter(output));
        EXPECT_LE(std::distance(begin, end), 2);
    });

    EXPECT_EQ(input, output);
}

TEST(BatchingTests, batchSizeBiggerThanInput)
{
    std::vector<int> const input{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::vector<int> output;

    util::forEachBatch(input, 20, [&](auto begin, auto end) {
        std::copy(begin, end, std::back_inserter(output));
        EXPECT_LE(std::distance(begin, end), 20);
    });

    EXPECT_EQ(input, output);
}

TEST(BatchingTests, emptyInput)
{
    std::vector<int> const input{};
    std::vector<int> output;

    util::forEachBatch(input, 20, [&](auto begin, auto end) {
        std::copy(begin, end, std::back_inserter(output));
        ASSERT_FALSE(true) << "Should not be called";
    });

    EXPECT_EQ(input, output);
}

namespace {

// Collects every batch handed to a util::BatchBuffer sink for later inspection.
struct BatchSink {
    std::vector<std::vector<int>> batches;

    auto
    fn()
    {
        return [this](std::vector<int> const& records) { batches.push_back(records); };
    }

    [[nodiscard]] std::vector<int>
    flattened() const
    {
        std::vector<int> all;
        for (auto const& batch : batches)
            all.insert(all.end(), batch.begin(), batch.end());
        return all;
    }
};

}  // namespace

TEST(BatchBufferTests, buffersBelowThresholdUntilFlush)
{
    BatchSink sink;
    util::BatchBuffer<int> buffer{3, sink.fn()};

    buffer.add({1});
    buffer.add({2});
    EXPECT_TRUE(sink.batches.empty()) << "Nothing should flush before the threshold";

    buffer.flush();
    ASSERT_EQ(sink.batches.size(), 1);
    EXPECT_EQ(sink.batches[0], (std::vector<int>{1, 2}));
}

TEST(BatchBufferTests, flushesExactlyOnThresholdWithNoRemainder)
{
    BatchSink sink;
    util::BatchBuffer<int> buffer{3, sink.fn()};

    buffer.add({1});
    buffer.add({2});
    buffer.add({3});  // reaches the threshold exactly
    ASSERT_EQ(sink.batches.size(), 1);
    EXPECT_EQ(sink.batches[0], (std::vector<int>{1, 2, 3}));

    buffer.flush();  // buffer already empty: no extra invocation
    EXPECT_EQ(sink.batches.size(), 1);
}

TEST(BatchBufferTests, flushesMultipleFullBatchesPlusRemainder)
{
    BatchSink sink;
    util::BatchBuffer<int> buffer{2, sink.fn()};

    for (int i = 1; i <= 5; ++i)
        buffer.add({i});

    EXPECT_EQ(sink.batches.size(), 2) << "Two full batches flushed inline";
    buffer.flush();
    ASSERT_EQ(sink.batches.size(), 3) << "Odd remainder flushed at the end";
    EXPECT_EQ(sink.flattened(), (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(BatchBufferTests, singleAddLargerThanThresholdFlushesWholeBufferAsOneBatch)
{
    BatchSink sink;
    util::BatchBuffer<int> buffer{3, sink.fn()};

    buffer.add({1, 2, 3, 4, 5});
    ASSERT_EQ(sink.batches.size(), 1);
    EXPECT_EQ(sink.batches[0], (std::vector<int>{1, 2, 3, 4, 5}));

    buffer.flush();
    EXPECT_EQ(sink.batches.size(), 1);
}

TEST(BatchBufferTests, flushOnEmptyBufferDoesNotInvokeSink)
{
    BatchSink sink;
    util::BatchBuffer<int> buffer{3, sink.fn()};

    buffer.flush();
    EXPECT_TRUE(sink.batches.empty());
}

TEST(BatchBufferTests, concurrentAddsConserveAllRecords)
{
    std::mutex mutex;
    std::size_t total = 0;
    util::BatchBuffer<int> buffer{10, [&](std::vector<int> const& records) {
                                      std::scoped_lock const lock{mutex};
                                      total += records.size();
                                  }};

    constexpr std::size_t kThreads = 8;
    constexpr std::size_t kAddsPerThread = 1'000;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (std::size_t t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            for (std::size_t i = 0; i < kAddsPerThread; ++i)
                buffer.add({1});
        });
    }
    for (auto& thread : threads)
        thread.join();

    buffer.flush();
    EXPECT_EQ(total, kThreads * kAddsPerThread) << "Every record must reach the sink exactly once";
}
