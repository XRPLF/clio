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
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace util;

static constexpr auto kLogFormat = "%Y-%m-%d %H:%M:%S.%f %^%3!l:%n%$ - %v";

struct BenchmarkLoggingInitializer {
    [[nodiscard]] static std::shared_ptr<spdlog::sinks::sink>
    createFileSink(std::string const& logDir, uint32_t sizeMB, uint32_t maxFiles)
    {
        return LogService::createFileSink(
            LogService::FileLoggingParams{
                .logDir = logDir,
                .rotation = LogService::RotationParams{.sizeMB = sizeMB, .maxFiles = maxFiles},
            },
            kLogFormat
        );
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
    std::string const dirName = fmt::format(
        "logs_{}", std::chrono::duration_cast<std::chrono::microseconds>(epochTime).count()
    );
    return tmpDir / "clio_benchmark" / dirName;
}

}  // anonymous namespace

static void
benchmarkConcurrentFileLogging(benchmark::State& state)
{
    auto const numThreads = static_cast<size_t>(state.range(0));
    auto const messagesPerThread = static_cast<size_t>(state.range(1));

    PrometheusService::init(config::getClioConfig());

    auto const logDir = uniqueLogDir();
    for (auto _ : state) {
        state.PauseTiming();

        std::filesystem::create_directories(logDir);
        static constexpr size_t kQueueSize = 8192;
        static constexpr size_t kThreadCount = 1;
        spdlog::init_thread_pool(kQueueSize, kThreadCount);

        auto fileSink = BenchmarkLoggingInitializer::createFileSink(logDir, 5, 25);

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
                Logger const threadLogger =
                    BenchmarkLoggingInitializer::getLogger(std::move(logger));

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
        state.SetIterationTime(
            std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count()
        );

        std::filesystem::remove_all(logDir);
    }

    auto const totalMessages = numThreads * messagesPerThread;
    state.counters["TotalMessagesRate"] =
        benchmark::Counter(totalMessages, benchmark::Counter::kIsRate);
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
