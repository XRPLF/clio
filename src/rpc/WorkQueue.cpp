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

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace rpc {

void
WorkQueue::OneTimeCallable::setCallable(std::function<void()> func)
{
    func_ = std::move(func);
}

void
WorkQueue::OneTimeCallable::operator()()
{
    if (not called_) {
        func_();
        called_ = true;
    }
}

WorkQueue::OneTimeCallable::
operator bool() const
{
    return func_.operator bool();
}

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
    ASSERT(not processingStarted_, "Attempt to start processing work queue more than once");
    processingStarted_ = true;

    // Spawn workers for all tasks that were queued before processing started
    auto const numTasks = size();
    for (auto i = 0uz; i < numTasks; ++i) {
        util::spawn(ioc_, [this](auto yield) { executeTask(yield); });
    }
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

    {
        auto state = queueState_.lock();
        state->push(priority, std::move(func));
    }

    ++curSize_.get();

    if (not processingStarted_)
        return true;

    util::spawn(ioc_, [this](auto yield) { executeTask(yield); });

    return true;
}

void
WorkQueue::requestStop(std::function<void()> onQueueEmpty)
{
    auto handler = onQueueEmpty_.lock();
    handler->setCallable(std::move(onQueueEmpty));

    stopping_ = true;
}

void
WorkQueue::stop()
{
    if (not stopping_.exchange(true))
        requestStop();

    ioc_.join();

    {
        auto onTasksComplete = onQueueEmpty_.lock();
        ASSERT(onTasksComplete->operator bool(), "onTasksComplete must be set when stopping is true.");
        onTasksComplete->operator()();
    }
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

void
WorkQueue::executeTask(boost::asio::yield_context yield)
{
    std::optional<TaskWithTimestamp> taskWithTimestamp;
    {
        auto state = queueState_.lock();
        taskWithTimestamp = state->popNext();
    }

    ASSERT(
        taskWithTimestamp.has_value(),
        "Queue should not be empty as we spawn a coro with executeTask for each postCoro."
    );
    auto const takenAt = std::chrono::system_clock::now();
    auto const waited =
        std::chrono::duration_cast<std::chrono::microseconds>(takenAt - taskWithTimestamp->queuedAt).count();

    ++queued_.get();
    durationUs_.get() += waited;
    LOG(log_.info()) << "WorkQueue wait time: " << waited << ", queue size: " << size();

    taskWithTimestamp->task(yield);
    --curSize_.get();
}

}  // namespace rpc
