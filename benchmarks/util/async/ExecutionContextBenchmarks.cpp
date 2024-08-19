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

#include "etl/ETLHelpers.hpp"
#include "util/Random.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/async/context/SyncExecutionContext.hpp"

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <latch>
#include <optional>
#include <thread>
#include <vector>

using namespace util;
using namespace util::async;

class TestThread {
    std::vector<std::thread> threads_;
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;

public:
    TestThread(std::vector<uint64_t> const& data) : q_(data.size()), res_(data.size())
    {
        for (auto el : data)
            q_.push(el);
    }

    ~TestThread()
    {
        for (auto& t : threads_) {
            if (t.joinable())
                t.join();
        }
    }

    void
    run(std::size_t numThreads)
    {
        std::latch completion{numThreads};
        for (std::size_t i = 0; i < numThreads; ++i) {
            q_.push(std::nullopt);
            threads_.emplace_back([this, &completion]() { process(completion); });
        }

        completion.wait();
    }

private:
    void
    process(std::latch& completion)
    {
        while (auto v = q_.pop()) {
            if (not v.has_value())
                break;

            res_.push(v.value() * v.value());
        }

        completion.count_down(1);
    }
};

template <typename CtxType>
class TestExecutionContextBatched {
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;
    std::size_t batchSize_;

public:
    TestExecutionContextBatched(std::vector<uint64_t> const& data, std::size_t batchSize = 5000u)
        : q_(data.size()), res_(data.size()), batchSize_(batchSize)
    {
        for (auto el : data)
            q_.push(el);
    }

    void
    run(std::size_t numThreads)
    {
        using OpType = typename CtxType::template StoppableOperation<void>;

        CtxType ctx{numThreads};
        std::vector<OpType> operations;

        for (std::size_t i = 0; i < numThreads; ++i) {
            q_.push(std::nullopt);

            operations.push_back(ctx.execute(
                [this](auto stopRequested) {
                    bool hasMore = true;
                    auto doOne = [this] {
                        auto v = q_.pop();
                        if (not v.has_value())
                            return false;

                        res_.push(v.value() * v.value());
                        return true;
                    };

                    while (not stopRequested and hasMore) {
                        for (std::size_t i = 0; i < batchSize_ and hasMore; ++i)
                            hasMore = doOne();
                    }
                },
                std::chrono::seconds{5}
            ));
        }

        for (auto& op : operations)
            op.wait();
    }
};

template <typename CtxType>
class TestAnyExecutionContextBatched {
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;
    std::size_t batchSize_;

public:
    TestAnyExecutionContextBatched(std::vector<uint64_t> const& data, std::size_t batchSize = 5000u)
        : q_(data.size()), res_(data.size()), batchSize_(batchSize)
    {
        for (auto el : data)
            q_.push(el);
    }

    void
    run(std::size_t numThreads)
    {
        CtxType ctx{numThreads};
        AnyExecutionContext anyCtx{ctx};
        std::vector<AnyOperation<void>> operations;

        for (std::size_t i = 0; i < numThreads; ++i) {
            q_.push(std::nullopt);

            operations.push_back(anyCtx.execute(
                [this](auto stopRequested) {
                    bool hasMore = true;
                    auto doOne = [this] {
                        auto v = q_.pop();
                        if (not v.has_value())
                            return false;

                        res_.push(v.value() * v.value());
                        return true;
                    };

                    while (not stopRequested and hasMore) {
                        for (std::size_t i = 0; i < batchSize_ and hasMore; ++i)
                            hasMore = doOne();
                    }
                },
                std::chrono::seconds{5}
            ));
        }

        for (auto& op : operations)
            op.wait();
    }
};

static auto
generateData()
{
    constexpr auto TOTAL = 10'000;
    std::vector<uint64_t> data;
    data.reserve(TOTAL);
    for (auto i = 0; i < TOTAL; ++i)
        data.push_back(util::Random::uniform(1, 100'000'000));

    return data;
}

static void
benchmarkThreads(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestThread t{data};
        t.run(state.range(0));
    }
}

template <typename CtxType>
void
benchmarkExecutionContextBatched(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestExecutionContextBatched<CtxType> t{data, state.range(1)};
        t.run(state.range(0));
    }
}

template <typename CtxType>
void
benchmarkAnyExecutionContextBatched(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContextBatched<CtxType> t{data, state.range(1)};
        t.run(state.range(0));
    }
}

// Simplest implementation using async queues and std::thread
BENCHMARK(benchmarkThreads)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

// Same implementation using each of the available execution contexts
BENCHMARK(benchmarkExecutionContextBatched<PoolExecutionContext>)
    ->ArgsProduct({
        {1, 2, 4, 8},             // threads
        {500, 1000, 5000, 10000}  // batch size
    });
BENCHMARK(benchmarkExecutionContextBatched<CoroExecutionContext>)
    ->ArgsProduct({
        {1, 2, 4, 8},             // threads
        {500, 1000, 5000, 10000}  // batch size
    });
BENCHMARK(benchmarkExecutionContextBatched<SyncExecutionContext>)
    ->ArgsProduct({
        {1, 2, 4, 8},             // threads
        {500, 1000, 5000, 10000}  // batch size
    });

// Same implementations going thru AnyExecutionContext
BENCHMARK(benchmarkAnyExecutionContextBatched<PoolExecutionContext>)
    ->ArgsProduct({
        {1, 2, 4, 8},             // threads
        {500, 1000, 5000, 10000}  // batch size
    });
BENCHMARK(benchmarkAnyExecutionContextBatched<CoroExecutionContext>)
    ->ArgsProduct({
        {1, 2, 4, 8},             // threads
        {500, 1000, 5000, 10000}  // batch size
    });
BENCHMARK(benchmarkAnyExecutionContextBatched<SyncExecutionContext>)
    ->ArgsProduct({
        {1, 2, 4, 8},             // threads
        {500, 1000, 5000, 10000}  // batch size
    });
