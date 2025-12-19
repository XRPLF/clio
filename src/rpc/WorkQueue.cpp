//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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
#include "util/Spawn.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace rpc {

WorkQueue::WorkQueue(DontStartProcessingTag, std::uint32_t numWorkers, uint32_t maxSize)
    : queued_{PrometheusService::counterInt(
          "work_queue_queued_total_number",
          util::prometheus::Labels(),
          "The total number of tasks queued for processing"
      )}
    , durationUs_{PrometheusService::counterInt(
          "work_queue_cumulative_tasks_duration_us",
          util::prometheus::Labels(),
          "The total number of microseconds tasks were waiting to be executed"
      )}
    , curSize_{PrometheusService::gaugeInt(
          "work_queue_current_size",
          util::prometheus::Labels(),
          "The current number of tasks in the queue"
      )}
    , ioc_{numWorkers}
    , strand_{ioc_.get_executor()}
    , waitTimer_(ioc_)
{
    if (maxSize != 0)
        maxSize_ = maxSize;
}

WorkQueue::WorkQueue(std::uint32_t numWorkers, uint32_t maxSize)
    : WorkQueue(kDONT_START_PROCESSING_TAG, numWorkers, maxSize)
{
    startProcessing();
}

WorkQueue::~WorkQueue()
{
    stop();
}

void
WorkQueue::startProcessing()
{
    util::spawn(strand_, [this](auto yield) {
        ASSERT(not hasDispatcher_, "Dispatcher already running");

        hasDispatcher_ = true;
        dispatcherLoop(yield);
    });
}

bool
WorkQueue::postCoro(TaskType func, bool isWhiteListed, Priority priority)
{
    if (stopping_) {
        LOG(log_.warn()) << "Queue is stopping, rejecting incoming task.";
        return false;
    }

    if (size() >= maxSize_ && !isWhiteListed) {
        LOG(log_.warn()) << "Queue is full. rejecting job. current size = " << size() << "; max size = " << maxSize_;
        return false;
    }

    ++curSize_.get();
    auto needsWakeup = false;

    {
        auto state = dispatcherState_.lock();

        needsWakeup = std::exchange(state->isIdle, false);

        state->push(priority, std::move(func));
    }

    if (needsWakeup)
        boost::asio::post(strand_, [this] { waitTimer_.cancel(); });

    return true;
}

void
WorkQueue::dispatcherLoop(boost::asio::yield_context yield)
{
    LOG(log_.info()) << "WorkQueue dispatcher starting";

    // all ongoing tasks must be completed before stopping fully
    while (not stopping_ or size() > 0) {
        std::optional<TaskType> task;

        {
            auto state = dispatcherState_.lock();

            if (state->empty()) {
                state->isIdle = true;
            } else {
                task = state->popNext();
            }
        }

        if (not stopping_ and not task.has_value()) {
            waitTimer_.expires_at(std::chrono::steady_clock::time_point::max());
            boost::system::error_code ec;
            waitTimer_.async_wait(yield[ec]);
        } else if (task.has_value()) {
            util::spawn(
                ioc_,
                [this, spawnedAt = std::chrono::system_clock::now(), task = std::move(*task)](auto yield) mutable {
                    auto const takenAt = std::chrono::system_clock::now();
                    auto const waited =
                        std::chrono::duration_cast<std::chrono::microseconds>(takenAt - spawnedAt).count();

                    ++queued_.get();
                    durationUs_.get() += waited;
                    LOG(log_.info()) << "WorkQueue wait time: " << waited << ", queue size: " << size();

                    task(yield);

                    --curSize_.get();
                }
            );
        }
    }

    LOG(log_.info()) << "WorkQueue dispatcher shutdown requested - time to execute onTasksComplete";

    {
        auto onTasksComplete = onQueueEmpty_.lock();
        ASSERT(onTasksComplete->operator bool(), "onTasksComplete must be set when stopping is true.");
        onTasksComplete->operator()();
    }

    LOG(log_.info()) << "WorkQueue dispatcher finished";
}

void
WorkQueue::requestStop(std::function<void()> onQueueEmpty)
{
    auto handler = onQueueEmpty_.lock();
    *handler = std::move(onQueueEmpty);

    stopping_ = true;
    auto needsWakeup = false;

    {
        auto state = dispatcherState_.lock();
        needsWakeup = std::exchange(state->isIdle, false);
    }

    if (needsWakeup)
        boost::asio::post(strand_, [this] { waitTimer_.cancel(); });
}

void
WorkQueue::stop()
{
    if (not stopping_.exchange(true))
        requestStop();

    ioc_.join();
}

WorkQueue
WorkQueue::makeWorkQueue(util::config::ClioConfigDefinition const& config)
{
    static util::Logger const log{"RPC"};  // NOLINT(readability-identifier-naming)
    auto const serverConfig = config.getObject("server");
    auto const numThreads = config.get<uint32_t>("workers");
    auto const maxQueueSize = serverConfig.get<uint32_t>("max_queue_size");

    LOG(log.info()) << "Number of workers = " << numThreads << ". Max queue size = " << maxQueueSize;
    return WorkQueue{numThreads, maxQueueSize};
}

boost::json::object
WorkQueue::report() const
{
    auto obj = boost::json::object{};

    obj["queued"] = queued_.get().value();
    obj["queued_duration_us"] = durationUs_.get().value();
    obj["current_queue_size"] = curSize_.get().value();
    obj["max_queue_size"] = maxSize_;

    return obj;
}

size_t
WorkQueue::size() const
{
    return curSize_.get().value();
}

}  // namespace rpc
