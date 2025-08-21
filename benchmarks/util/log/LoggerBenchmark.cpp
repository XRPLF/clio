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

#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <benchmark/benchmark.h>
#include <fmt/format.h>
#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/spdlog.h>

#include <barrier>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace util;

static constexpr auto kLOG_FORMAT = "%Y-%m-%d %H:%M:%S.%f %^%3!l:%n%$ - %v";

struct BenchmarkLoggingInitializer {
    static std::shared_ptr<spdlog::sinks::sink>
    createFileSink(LogService::FileLoggingParams const& params)
    {
        return LogService::createFileSink(params, kLOG_FORMAT);
    }

    static Logger
    getLogger(std::shared_ptr<spdlog::logger> logger)
    {
        return Logger(std::move(logger));
    }
};

namespace {

std::string
uniqueLogDir()
{
    auto const epochTime = std::chrono::high_resolution_clock::now().time_since_epoch();
    auto const tmpDir = std::filesystem::temp_directory_path();
    std::string const dirName =
        fmt::format("logs_{}", std::chrono::duration_cast<std::chrono::microseconds>(epochTime).count());
    return tmpDir / "clio_benchmark" / dirName;
}

}  // anonymous namespace

static void
benchmarkConcurrentFileLogging(benchmark::State& state)
{
    spdlog::drop_all();

    auto const numThreads = static_cast<size_t>(state.range(0));
    auto const messagesPerThread = static_cast<size_t>(state.range(1));

    PrometheusService::init(config::getClioConfig());

    auto const logDir = uniqueLogDir();
    for (auto _ : state) {
        state.PauseTiming();

        std::filesystem::create_directories(logDir);
        spdlog::init_thread_pool(8192, 1);

        auto fileSink = BenchmarkLoggingInitializer::createFileSink({
            .logDir = logDir,
            .rotationSizeMB = 5,
            .dirMaxFiles = 25,
        });

        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        std::chrono::high_resolution_clock::time_point start;
        std::barrier barrier(numThreads, [&state, &start]() {
            state.ResumeTiming();
            start = std::chrono::high_resolution_clock::now();
        });

        for (size_t threadNum = 0; threadNum < numThreads; ++threadNum) {
            threads.emplace_back([threadNum, messagesPerThread, fileSink, &barrier]() {
                std::string const channel = fmt::format("Thread_{}", threadNum);
                auto logger = std::make_shared<spdlog::async_logger>(
                    channel, fileSink, spdlog::thread_pool(), spdlog::async_overflow_policy::block
                );
                spdlog::register_logger(logger);
                Logger const threadLogger = BenchmarkLoggingInitializer::getLogger(std::move(logger));

                barrier.arrive_and_wait();

                for (size_t messageNum = 0; messageNum < messagesPerThread; ++messageNum) {
                    LOG(threadLogger.info()) << "Test log message #" << messageNum;
                }
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }
        spdlog::shutdown();

        auto const end = std::chrono::high_resolution_clock::now();
        state.SetIterationTime(std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count());

        std::filesystem::remove_all(logDir);
    }

    auto const totalMessages = numThreads * messagesPerThread;
    state.counters["TotalMessagesRate"] = benchmark::Counter(totalMessages, benchmark::Counter::kIsRate);
    state.counters["Threads"] = numThreads;
    state.counters["MessagesPerThread"] = messagesPerThread;
}

// One line of log message is around 110 bytes
// So, 100K messages is around 10.5MB

BENCHMARK(benchmarkConcurrentFileLogging)
    ->ArgsProduct({
        // Number of threads
        {1, 2, 4, 8},
        // Messages per thread
        {10'000, 100'000, 500'000, 1'000'000, 10'000'000},
    })
    ->UseManualTime()
    ->Unit(benchmark::kMillisecond);
