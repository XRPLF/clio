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
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>

using namespace rpc;
using namespace util::config;

namespace {

auto const kCONFIG = ClioConfigDefinition{
    {"prometheus.compress_reply", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
    {"prometheus.enabled", ConfigValue{ConfigType::Boolean}.defaultValue(true)},
    {"log.channels.[].channel", Array{ConfigValue{ConfigType::String}}},
    {"log.channels.[].level", Array{ConfigValue{ConfigType::String}}},
    {"log.level", ConfigValue{ConfigType::String}.defaultValue("info")},
    {"log.format", ConfigValue{ConfigType::String}.defaultValue(R"(%Y-%m-%d %H:%M:%S.%f %^%3!l:%n%$ - %v)")},
    {"log.is_async", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
    {"log.enable_console", ConfigValue{ConfigType::Boolean}.defaultValue(false)},
    {"log.directory", ConfigValue{ConfigType::String}.optional()},
    {"log.rotation_size", ConfigValue{ConfigType::Integer}.defaultValue(2048).withConstraint(gValidateUint32)},
    {"log.directory_max_files", ConfigValue{ConfigType::Integer}.defaultValue(25).withConstraint(gValidateUint32)},
    {"log.tag_style", ConfigValue{ConfigType::String}.defaultValue("none")},
};

// this should be a fixture but it did not work with Args very well
void
init()
{
    static std::once_flag kONCE;
    std::call_once(kONCE, [] {
        PrometheusService::init(kCONFIG);
        (void)util::LogService::init(kCONFIG);
    });
}

}  // namespace

static void
benchmarkWorkQueue(benchmark::State& state)
{
    init();

    auto const total = static_cast<size_t>(state.range(0));
    auto const numThreads = static_cast<uint32_t>(state.range(1));
    auto const maxSize = static_cast<uint32_t>(state.range(2));
    auto const delayMs = static_cast<uint32_t>(state.range(3));

    for (auto _ : state) {
        std::atomic_size_t totalExecuted = 0uz;
        std::atomic_size_t totalQueued = 0uz;

        state.PauseTiming();
        WorkQueue queue(numThreads, maxSize);
        state.ResumeTiming();

        for (auto i = 0uz; i < total; ++i) {
            totalQueued += static_cast<std::size_t>(queue.postCoro(
                [&delayMs, &totalExecuted](auto yield) {
                    ++totalExecuted;

                    boost::asio::steady_timer timer(yield.get_executor(), std::chrono::milliseconds{delayMs});
                    timer.async_wait(yield);
                },
                /* isWhiteListed = */ false
            ));
        }

        queue.stop();

        ASSERT(totalExecuted == totalQueued, "Totals don't match");
        ASSERT(totalQueued <= total, "Queued more than requested");
        ASSERT(totalQueued >= maxSize, "Queued less than maxSize");
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
    ->ArgsProduct({{1'000, 10'000, 100'000}, {2, 4, 8}, {0, 5'000}, {10, 100, 250}})
    ->Unit(benchmark::kMillisecond);
