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

#pragma once

#include "util/Mutex.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <utility>

namespace rpc {

/**
 * @brief An interface for any class providing a report as json object
 */
struct Reportable {
    virtual ~Reportable() = default;

    /**
     * @brief Generate a report of the work queue state.
     *
     * @return The report as a JSON object.
     */
    [[nodiscard]] virtual boost::json::object
    report() const = 0;
};

/**
 * @brief An asynchronous, thread-safe queue for RPC requests.
 */
class WorkQueue : public Reportable {
    using TaskType = std::function<void(boost::asio::yield_context)>;
    using QueueType = std::queue<TaskType>;

public:
    /**
     * @brief Represents a task scheduling priority
     */
    enum class Priority : uint8_t {
        High,
        Default,
    };

private:
    struct DispatcherState {
        QueueType high;
        QueueType normal;

        bool isIdle = false;
        size_t highPriorityCounter = 0;

        void
        push(Priority priority, auto&& task)
        {
            auto& queue = [this, priority] -> QueueType& {
                if (priority == Priority::High)
                    return high;
                return normal;
            }();
            queue.push(std::forward<decltype(task)>(task));
        }

        [[nodiscard]] bool
        empty() const
        {
            return high.empty() and normal.empty();
        }

        [[nodiscard]] std::optional<TaskType>
        popNext()
        {
            if (not high.empty() and (highPriorityCounter < kTAKE_HIGH_PRIO or normal.empty())) {
                auto task = std::move(high.front());
                high.pop();
                ++highPriorityCounter;
                return task;
            }

            if (not normal.empty()) {
                auto task = std::move(normal.front());
                normal.pop();
                highPriorityCounter = 0;
                return task;
            }

            return std::nullopt;
        }
    };

private:
    static constexpr auto kTAKE_HIGH_PRIO = 4uz;

    // these are cumulative for the lifetime of the process
    std::reference_wrapper<util::prometheus::CounterInt> queued_;
    std::reference_wrapper<util::prometheus::CounterInt> durationUs_;

    std::reference_wrapper<util::prometheus::GaugeInt> curSize_;
    uint32_t maxSize_ = std::numeric_limits<uint32_t>::max();

    util::Logger log_{"RPC"};
    boost::asio::thread_pool ioc_;
    boost::asio::strand<boost::asio::thread_pool::executor_type> strand_;
    bool hasDispatcher_ = false;

    std::atomic_bool stopping_;

    util::Mutex<std::function<void()>> onQueueEmpty_;
    util::Mutex<DispatcherState> dispatcherState_;
    boost::asio::steady_timer waitTimer_;

public:
    struct DontStartProcessingTag {};
    static constexpr DontStartProcessingTag kDONT_START_PROCESSING_TAG = {};

    /**
     * @brief Create an instance of the work queue.
     *
     * The work queue immediately starts to process tasks as they come.
     *
     * @param numWorkers The amount of threads to spawn in the pool
     * @param maxSize The maximum capacity of the queue; 0 means unlimited
     */
    WorkQueue(std::uint32_t numWorkers, uint32_t maxSize = 0);

    /**
     * @brief Create an instance of the work queue without starting the processing of events.
     *
     * Clients are expected to call `startProcessing` manually once ready to start processing tasks.
     *
     * @param numWorkers The amount of threads to spawn in the pool
     * @param maxSize The maximum capacity of the queue; 0 means unlimited
     */
    WorkQueue(DontStartProcessingTag, std::uint32_t numWorkers, uint32_t maxSize = 0);

    ~WorkQueue() override;

    /**
     * @brief Start processing of the enqueued tasks.
     */
    void
    startProcessing();

    /**
     * @brief Put the work queue into a stopping state. This will prevent new jobs from being queued.
     *
     * @param onQueueEmpty A callback to run when the last task in the queue is completed
     */
    void
    requestStop(std::function<void()> onQueueEmpty = [] {});

    /**
     * @brief Put the work queue into a stopping state and await workers to finish.
     */
    void
    stop();

    /**
     * @brief A factory function that creates the work queue based on a config.
     *
     * @param config The Clio config to use
     * @return The work queue
     */
    [[nodiscard]] static WorkQueue
    makeWorkQueue(util::config::ClioConfigDefinition const& config);

    /**
     * @brief Submit a job to the work queue.
     *
     * The job will be rejected if isWhiteListed is set to false and the current size of the queue reached capacity.
     *
     * @param func The function object to queue as a job
     * @param isWhiteListed Whether the queue capacity applies to this job
     * @param priority The priority of the task
     * @return true if the job was successfully queued; false otherwise
     */
    bool
    postCoro(TaskType func, bool isWhiteListed, Priority priority = Priority::Default);

    /**
     * @brief Generate a report of the work queue state.
     *
     * @return The report as a JSON object.
     */
    [[nodiscard]] boost::json::object
    report() const override;

    /**
     * @brief Wait until all the jobs in the queue are finished.
     */
    void
    join();

    /**
     * @brief Get the size of the queue.
     *
     * @return The number of jobs in the queue.
     */
    [[nodiscard]] size_t
    size() const;

private:
    void
    dispatcherLoop(boost::asio::yield_context yield);
};

}  // namespace rpc
