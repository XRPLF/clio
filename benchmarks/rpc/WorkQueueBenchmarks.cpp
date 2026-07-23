#include "rpc/WorkQueue.hpp"
#include "util/Assert.hpp"
#include "util/config/Array.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <benchmark/benchmark.h>
#include <boost/asio/steady_timer.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

using namespace rpc;
using namespace util::config;

namespace {

auto const kConfig = ClioConfigDefinition{
    {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
    {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
    {"log.channels.[].channel", Array{ConfigValue{ConfigType::String}}},
    {"log.channels.[].level", Array{ConfigValue{ConfigType::String}}},
    {"log.level", ConfigValue{ConfigType::String}.defaultValue("info")},
    {"log.format",
     ConfigValue{ConfigType::String}.defaultValue(R"(%Y-%m-%d %H:%M:%S.%f %^%3!l:%n%$ - %v)")},
    {"log.is_async", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
    {"log.enable_console", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
    {"log.directory", ConfigValue{ConfigType::String}.optional()},
    {"log.rotation_size",
     ConfigValue{ConfigType::Integer}.defaultValue(2048).withConstraint(gValidateUint32)},
    {"log.directory_max_files",
     ConfigValue{ConfigType::Integer}.defaultValue(25).withConstraint(gValidateUint32)},
    {"log.tag_style", ConfigValue{ConfigType::String}.defaultValue("none")},
};

// this should be a fixture but it did not work with Args very well
void
init()
{
    static std::once_flag kOnce;
    std::call_once(kOnce, [] {
        PrometheusService::init(kConfig);
        (void)util::LogService::init(kConfig);
    });
}

}  // namespace

static void
benchmarkWorkQueue(benchmark::State& state)
{
    init();

    auto const wqThreads = static_cast<uint32_t>(state.range(0));
    auto const maxQueueSize = static_cast<uint32_t>(state.range(1));
    auto const clientThreads = static_cast<uint32_t>(state.range(2));
    auto const itemsPerClient = static_cast<uint32_t>(state.range(3));
    auto const clientProcessingMs = static_cast<uint32_t>(state.range(4));

    for (auto _ : state) {
        std::atomic_size_t totalExecuted = 0uz;
        std::atomic_size_t totalQueued = 0uz;

        state.PauseTiming();
        WorkQueue queue(wqThreads, maxQueueSize);
        state.ResumeTiming();

        std::vector<std::thread> threads;
        threads.reserve(clientThreads);

        for (auto t = 0uz; t < clientThreads; ++t) {
            threads.emplace_back([&] {
                for (auto i = 0uz; i < itemsPerClient; ++i) {
                    totalQueued += static_cast<std::size_t>(queue.postCoro(
                        [&clientProcessingMs, &totalExecuted](auto yield) {
                            ++totalExecuted;

                            boost::asio::steady_timer timer(
                                yield.get_executor(), std::chrono::milliseconds{clientProcessingMs}
                            );
                            timer.async_wait(yield);

                            std::this_thread::sleep_for(std::chrono::microseconds{10});
                        },
                        /* isWhiteListed = */ false
                    ));
                }
            });
        }

        for (auto& t : threads)
            t.join();

        queue.stop();

        ASSERT(totalExecuted == totalQueued, "Totals don't match");
        ASSERT(totalQueued <= itemsPerClient * clientThreads, "Queued more than requested");

        if (maxQueueSize == 0) {
            ASSERT(
                totalQueued == itemsPerClient * clientThreads, "Queued exactly the expected amount"
            );
        } else {
            ASSERT(
                totalQueued >= std::min(maxQueueSize, itemsPerClient * clientThreads),
                "Queued less than expected"
            );
        }
    }
}

// Usage example:
/*
  ./clio_benchmark \
    --benchmark_repetitions=10 \
    --benchmark_display_aggregates_only=true \
    --benchmark_min_time=1x \
    --benchmark_filter="WorkQueue"
*/
// TODO: figure out what happens on 1 thread
BENCHMARK(benchmarkWorkQueue)
    ->ArgsProduct({{2, 4, 8, 16}, {0, 5'000}, {4, 8, 16}, {1'000, 10'000}, {10, 100, 250}})
    ->Unit(benchmark::kMillisecond);
