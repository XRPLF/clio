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

#include "etl/ETLHelpers.h"
#include "util/Random.h"
#include "util/async/AnyExecutionContext.h"
#include "util/async/AnyOperation.h"
#include "util/async/context/BasicExecutionContext.h"
#include "util/async/context/SyncExecutionContext.h"

#include <benchmark/benchmark.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <latch>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace util;
using namespace util::async;

class TestThread {
    std::vector<std::thread> threads_;
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;

public:
    TestThread(std::vector<uint64_t> const& data) : q_(data.size() + 1), res_(data.size() + 1)
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

class TestCoroExecutionContext {
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;
    std::size_t batchSize_;

public:
    TestCoroExecutionContext(std::vector<uint64_t> const& data, std::size_t batchSize = 5000u)
        : q_(data.size()), res_(data.size()), batchSize_(batchSize)
    {
        for (auto el : data)
            q_.push(el);
    }

    void
    run(std::size_t numThreads)
    {
        using CtxType = CoroExecutionContext;

        CtxType ctx{numThreads};
        std::vector<CtxType::StoppableOperation<void>> operations;

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
                        for (std::size_t i = 0; i < batchSize_; ++i)
                            hasMore = doOne();
                    }
                },
                std::chrono::seconds{1}
            ));
        }

        for (auto& op : operations)
            op.wait();
    }
};

class TestCoroExecutionContextStrand {
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;
    std::size_t batchSize_;

public:
    TestCoroExecutionContextStrand(std::vector<uint64_t> const& data, std::size_t batchSize = 5000u)
        : q_(data.size()), res_(data.size()), batchSize_(batchSize)
    {
        for (auto el : data)
            q_.push(el);
    }

    void
    run(std::size_t numThreads)
    {
        using CtxType = CoroExecutionContext;

        CtxType ctx{numThreads};
        std::vector<CtxType::StoppableOperation<void>> operations;
        auto strand = ctx.makeStrand();

        for (std::size_t i = 0; i < numThreads; ++i) {
            q_.push(std::nullopt);

            operations.push_back(strand.execute([this](auto stopRequested) {
                bool hasMore = true;
                auto doOne = [this] {
                    auto v = q_.pop();
                    if (not v.has_value())
                        return false;

                    res_.push(v.value() * v.value());
                    return true;
                };

                while (not stopRequested and hasMore) {
                    for (std::size_t i = 0; i < batchSize_ && hasMore; ++i)
                        hasMore = doOne();
                }
            }));
        }

        for (auto& op : operations)
            op.wait();
    }
};

class TestCoroExecutionContextStrandWithReturn {
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;
    std::size_t batchSize_;

public:
    TestCoroExecutionContextStrandWithReturn(std::vector<uint64_t> const& data, std::size_t batchSize = 5000u)
        : q_(data.size()), res_(data.size()), batchSize_(batchSize)
    {
        for (auto el : data)
            q_.push(el);
    }

    void
    run(std::size_t numThreads)
    {
        using CtxType = CoroExecutionContext;

        CtxType ctx{numThreads};
        std::vector<CtxType::StoppableOperation<int>> operations;
        auto strand = ctx.makeStrand();

        for (std::size_t i = 0; i < numThreads; ++i) {
            q_.push(std::nullopt);

            operations.push_back(strand.execute([this](auto stopRequested) {
                bool hasMore = true;
                auto doOne = [this] {
                    auto v = q_.pop();
                    if (not v.has_value())
                        return false;

                    res_.push(v.value() * v.value());
                    return true;
                };

                while (not stopRequested and hasMore) {
                    for (std::size_t i = 0; i < batchSize_ && hasMore; ++i)
                        hasMore = doOne();
                }

                return 1234;
            }));
        }

        for (auto& op : operations) {
            if (auto v = op.get(); v) {
                if (v.value() != 1234)
                    throw std::logic_error("oops");
            } else {
                std::cout << "got err: " << v.error() << '\n';
            }
        }
    }
};

struct TestSleepingCoroExecutionContext {
    static void
    run(std::size_t numThreads, std::size_t numTasks)
    {
        using CtxType = CoroExecutionContext;
        CtxType ctx{numThreads};
        std::vector<CtxType::Operation<void>> futures;
        std::latch completion{numTasks};

        for (std::size_t i = 0; i < numTasks; ++i) {
            futures.push_back(ctx.execute([&completion]() {
                std::this_thread::sleep_for(std::chrono::nanoseconds{1});
                completion.count_down(1);
            }));
        }

        completion.wait();
    }
};

struct TestSleepingWithStopTokenCoroExecutionContext {
    static void
    run(std::size_t numThreads, std::size_t numTasks)
    {
        using CtxType = CoroExecutionContext;
        CtxType ctx{numThreads};
        std::vector<CtxType::StoppableOperation<void>> futures;
        std::latch completion{numTasks};
        auto const batchSize = numTasks / numThreads;

        futures.reserve(numThreads);
        for (auto i = 0u; i < numThreads; ++i) {
            futures.push_back(ctx.execute([&completion, batchSize](auto stopRequested) {
                for (auto b = 0u; b < batchSize and not stopRequested; ++b) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds{1});
                    completion.count_down(1);
                }
            }));
        }

        completion.wait();
    }
};

struct TestSleepingThread {
    static void
    run(std::size_t numThreads, std::size_t numTasks)
    {
        std::vector<std::thread> threads;
        std::latch completion{numThreads};
        auto const batchSize = numTasks / numThreads;

        threads.reserve(numThreads);
        for (auto i = 0u; i < numThreads; ++i) {
            threads.emplace_back([&completion, batchSize]() {
                for (auto b = 0u; b < batchSize; ++b) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds{1});
                }
                completion.count_down(1);
            });
        }

        completion.wait();
        for (auto& t : threads) {
            if (t.joinable())
                t.join();
        }
    }
};

class TestAnyExecutionContext {
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;
    std::size_t batchSize_;

public:
    TestAnyExecutionContext(std::vector<uint64_t> const& data, std::size_t batchSize = 5000u)
        : q_(data.size()), res_(data.size()), batchSize_(batchSize)
    {
        for (auto el : data)
            q_.push(el);
    }

    template <typename ExecutionContextType>
    void
    run(std::size_t numThreads)
    {
        ExecutionContextType ctx{numThreads};
        AnyExecutionContext anyCtx{ctx};

        std::vector<AnyOperation<void>> futures;
        std::latch completion{numThreads};

        for (std::size_t i = 0; i < numThreads; ++i) {
            q_.push(std::nullopt);

            futures.push_back(anyCtx.execute([this, &completion](auto stopRequested) {
                bool done = false;
                while (not stopRequested && not done) {
                    // note: batches need to be of reasonable size.
                    // too small and you will lose on context switching;
                    // too big and cancellation will be slow.
                    for (std::size_t i = 0; i < batchSize_; ++i) {
                        auto v = q_.pop();
                        if (not v.has_value()) {
                            done = true;
                            break;
                        }

                        res_.push(v.value() * v.value());
                    }
                }
                completion.count_down(1);
            }));
        }

        completion.wait();
        for (auto& f : futures)
            f.wait();
    }
};

class TestAnyExecutionContextTimer {
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;
    std::size_t batchSize_;

public:
    TestAnyExecutionContextTimer(std::vector<uint64_t> const& data, std::size_t batchSize = 5000u)
        : q_(data.size()), res_(data.size()), batchSize_(batchSize)
    {
        for (auto el : data)
            q_.push(el);
    }

    template <typename ExecutionContextType>
    void
    run(std::size_t numThreads)
    {
        ExecutionContextType ctx{numThreads};
        AnyExecutionContext anyCtx{ctx};

        std::vector<AnyOperation<int>> futures;
        std::latch completion{numThreads};

        for (std::size_t i = 0; i < numThreads; ++i) {
            q_.push(std::nullopt);

            futures.push_back(anyCtx.execute(
                [this, &completion](auto stopRequested) {
                    bool done = false;
                    while (not stopRequested && not done) {
                        // note: batches need to be of reasonable size.
                        // too small and you will lose on context switching;
                        // too big and cancellation will be slow.
                        for (std::size_t i = 0; i < batchSize_; ++i) {
                            auto v = q_.pop();
                            if (not v.has_value()) {
                                done = true;
                                break;
                            }

                            res_.push(v.value() * v.value());
                        }
                    }
                    completion.count_down(1);
                    return 0;
                },
                std::chrono::seconds{1}
            ));
        }

        completion.wait();
        for (auto& f : futures) {
            auto v = f.get();
            if (v.value() != 0)
                throw std::logic_error("oops");
        }
    }
};

class TestAnyExecutionContext2 {
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;
    std::size_t batchSize_;

public:
    TestAnyExecutionContext2(std::vector<uint64_t> const& data, std::size_t batchSize = 5000u)
        : q_(data.size()), res_(data.size()), batchSize_(batchSize)
    {
        for (auto el : data)
            q_.push(el);
    }

    template <typename ExecutionContextType>
    void
    run(std::size_t numThreads)
    {
        ExecutionContextType ctx{numThreads};
        AnyExecutionContext anyCtx{ctx};

        std::vector<AnyOperation<uint64_t>> futures;
        std::latch completion{numThreads};

        for (std::size_t i = 0; i < numThreads; ++i) {
            q_.push(std::nullopt);

            futures.push_back(anyCtx.execute(
                [this, &completion](auto stopRequested) {
                    bool done = false;
                    while (not stopRequested and not done) {
                        for (std::size_t i = 0; i < batchSize_; ++i) {
                            auto v = q_.pop();
                            if (not v.has_value()) {
                                done = true;
                                break;
                            }

                            res_.push(v.value() * v.value());
                        }
                    }
                    completion.count_down(1);
                    return uint64_t{1234};
                },
                std::chrono::seconds{1}
            ));
        }

        completion.wait();
        for (auto& f : futures) {
            auto v = f.get();
            if (v.value() != 1234)
                throw std::logic_error("oops");
        }
    }
};

class TestAnyExecutionContextStrand {
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;
    std::size_t batchSize_;

public:
    TestAnyExecutionContextStrand(std::vector<uint64_t> const& data, std::size_t batchSize = 5000u)
        : q_(data.size()), res_(data.size()), batchSize_(batchSize)
    {
        for (auto el : data)
            q_.push(el);
    }

    template <typename ExecutionContextType>
    void
    run(std::size_t numThreads)
    {
        ExecutionContextType ctx{numThreads};
        auto anyCtx = AnyExecutionContext{ctx};
        std::vector<AnyOperation<int>> operations;
        auto strand = anyCtx.makeStrand();

        for (std::size_t i = 0; i < numThreads; ++i) {
            q_.push(std::nullopt);

            operations.push_back(strand.execute([this](auto stopRequested) {
                bool hasMore = true;
                auto doOne = [this] {
                    auto v = q_.pop();
                    if (not v.has_value())
                        return false;

                    res_.push(v.value() * v.value());
                    return true;
                };

                while (not stopRequested and hasMore) {
                    for (std::size_t i = 0; i < batchSize_ && hasMore; ++i)
                        hasMore = doOne();
                }

                return 1234;
            }));
        }

        for (auto& op : operations) {
            if (auto v = op.get(); v) {
                if (v.value() != 1234)
                    throw std::logic_error("oops");
            } else {
                std::cout << "got err: " << v.error() << '\n';
            }
        }
    }
};

class TestAnyExecutionContextNoToken {
    etl::ThreadSafeQueue<std::optional<uint64_t>> q_;
    etl::ThreadSafeQueue<uint64_t> res_;
    std::size_t batchSize_;

public:
    TestAnyExecutionContextNoToken(std::vector<uint64_t> const& data, std::size_t batchSize = 5000u)
        : q_(data.size()), res_(data.size()), batchSize_(batchSize)
    {
        for (auto el : data)
            q_.push(el);
    }

    template <typename ExecutionContextType>
    void
    run(std::size_t numThreads)
    {
        ExecutionContextType ctx{numThreads};
        AnyExecutionContext anyCtx{ctx};

        std::vector<AnyOperation<void>> futures;
        std::latch completion{numThreads};

        for (std::size_t i = 0; i < numThreads; ++i) {
            q_.push(std::nullopt);

            futures.push_back(anyCtx.execute([this, &completion]() {
                bool done = false;
                while (not done) {
                    // note: batches need to be of reasonable size.
                    // too small and you will lose on context switching;
                    // too big and cancellation will be slow.
                    for (std::size_t i = 0; i < batchSize_; ++i) {
                        auto v = q_.pop();
                        if (not v.has_value()) {
                            done = true;
                            break;
                        }

                        res_.push(v.value() * v.value());
                    }
                }
                completion.count_down(1);
            }));
        }

        completion.wait();
        for (auto& f : futures)
            f.wait();
    }
};

struct TestTimer {
    static void
    run(std::size_t numThreads)
    {
        using CtxType = CoroExecutionContext;

        CtxType ctx{numThreads};
        AnyExecutionContext anyCtx{ctx};
        std::latch completion{3};

        auto t1 = anyCtx.scheduleAfter(std::chrono::seconds{3}, [&completion](auto) {
            std::cout << "running timer without bool\n";
            completion.count_down(1);
        });
        auto t2 = anyCtx.scheduleAfter(
            std::chrono::seconds{5},
            [&completion]([[maybe_unused]] auto stopRequested, auto cancelled) {
                std::cout << "running timer with bool: " << cancelled << '\n';
                completion.count_down(1);
            }
        );
        auto t3 =
            anyCtx.scheduleAfter(std::chrono::seconds{1}, [&completion, &t2]([[maybe_unused]] auto stopRequested) {
                std::cout << "cancelling timer t2\n";
                t2.cancel();
                completion.count_down(1);
            });

        completion.wait();
    }
};

struct TestSync {
    static void
    run()
    {
        SyncExecutionContext ctx{};
        AnyExecutionContext anyCtx{ctx};
        std::latch completion{3};

        auto t1 = anyCtx.scheduleAfter(std::chrono::seconds{3}, [&completion]([[maybe_unused]] auto stopRequested) {
            std::cout << "running timer without bool\n";
            completion.count_down(1);
        });

        auto op1 = anyCtx.execute([] { std::cout << "unstoppable job 1 ran..\n"; });

        auto t2 = anyCtx.scheduleAfter(
            std::chrono::seconds{5},
            [&completion]([[maybe_unused]] auto stopRequested, auto cancelled) {
                std::cout << "running timer with bool: " << cancelled << '\n';
                completion.count_down(1);
            }
        );

        auto op2 = anyCtx.execute([] { std::cout << "unstoppable job 2 ran..\n"; });

        auto t3 =
            anyCtx.scheduleAfter(std::chrono::seconds{1}, [&completion, &t2]([[maybe_unused]] auto stopRequested) {
                std::cout << "cancelling timer t2\n";
                t2.cancel();
                completion.count_down(1);
            });

        completion.wait();
    }
};

static constexpr auto
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

static void
benchmarkCoroExecutionContext(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestCoroExecutionContext t{data};
        t.run(state.range(0));
    }
}

static void
benchmarkCoroExecutionContextStrand(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestCoroExecutionContextStrand t{data};
        t.run(state.range(0));
    }
}

static void
benchmarkCoroExecutionContextStrandWithReturn(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestCoroExecutionContextStrandWithReturn t{data};
        t.run(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_Future(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContext t{data};
        t.run<CoroExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_FutureTimeout(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContextTimer t{data};
        t.run<CoroExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_FutureNoToken(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContextNoToken t{data};
        t.run<CoroExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_Pool(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContext t{data};
        t.run<PoolExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_PoolTimeout(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContextTimer t{data};
        t.run<PoolExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_PoolNoToken(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContextNoToken t{data};
        t.run<PoolExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_Sync(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContext t{data};
        t.run<SyncExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_SyncTimeout(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContextTimer t{data};
        t.run<SyncExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_SyncNoToken(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContextNoToken t{data};
        t.run<SyncExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_2(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContext2 t{data};
        t.run<CoroExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_FutureStrand(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContextStrand t{data};
        t.run<CoroExecutionContext>(state.range(0));
    }
}

static void
benchmarkAnyExecutionContext_PoolStrand(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestAnyExecutionContextStrand t{data};
        t.run<PoolExecutionContext>(state.range(0));
    }
}

static void
benchmarkCoroExecutionContextBatches(benchmark::State& state)
{
    auto data = generateData();
    for (auto _ : state) {
        TestCoroExecutionContext t{data, state.range(1)};
        t.run(state.range(0));
    }
}

static void
benchmarkCoroExecutionContextSimpleSleep(benchmark::State& state)
{
    for (auto _ : state) {
        TestSleepingCoroExecutionContext::run(state.range(0), 10000);
    }
}

static void
benchmarkCoroExecutionContextBatchedSleep(benchmark::State& state)
{
    for (auto _ : state) {
        TestSleepingWithStopTokenCoroExecutionContext::run(state.range(0), 10000);
    }
}

static void
benchmarkThreadSleep(benchmark::State& state)
{
    for (auto _ : state) {
        TestSleepingThread::run(state.range(0), 10000);
    }
}

static void
benchmarkTimer(benchmark::State& state)
{
    for (auto _ : state) {
        TestTimer::run(state.range(0));
    }
}

static void
benchmarkSync(benchmark::State& state)
{
    for (auto _ : state) {
        TestSync::run();
    }
}

BENCHMARK(benchmarkThreads)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkCoroExecutionContext)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkCoroExecutionContextStrand)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkCoroExecutionContextStrandWithReturn)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_Future)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_FutureTimeout)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_FutureNoToken)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_Pool)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_PoolTimeout)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_PoolNoToken)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_Sync)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_SyncTimeout)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_SyncNoToken)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_2)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_FutureStrand)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkAnyExecutionContext_PoolStrand)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkCoroExecutionContextBatches)
    ->ArgsProduct({
        {1, 2, 4, 8},             // threads
        {500, 1000, 5000, 10000}  // batch size
    });
BENCHMARK(benchmarkCoroExecutionContextSimpleSleep)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkCoroExecutionContextBatchedSleep)->Arg(1)->Arg(2)->Arg(4)->Arg(8);
BENCHMARK(benchmarkThreadSleep)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

BENCHMARK(benchmarkTimer)->Arg(1)->Arg(2)->Arg(4)->Arg(8);

BENCHMARK(benchmarkSync);

BENCHMARK_MAIN();
