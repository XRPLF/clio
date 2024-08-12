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

#include "util/Assert.hpp"
#include "util/Mutex.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

namespace rpc {

/**
 * @brief An asynchronous, thread-safe queue for RPC requests.
 */
class WorkQueue {
    // these are cumulative for the lifetime of the process
    std::reference_wrapper<util::prometheus::CounterInt> queued_;
    std::reference_wrapper<util::prometheus::CounterInt> durationUs_;

    std::reference_wrapper<util::prometheus::GaugeInt> curSize_;
    uint32_t maxSize_ = std::numeric_limits<uint32_t>::max();

    util::Logger log_{"RPC"};
    boost::asio::thread_pool ioc_;

    std::atomic_bool stopping_;
    util::Mutex<std::function<void()>> onQueueEmpty_;

public:
    /**
     * @brief Create an we instance of the work queue.
     *
     * @param numWorkers The amount of threads to spawn in the pool
     * @param maxSize The maximum capacity of the queue; 0 means unlimited
     */
    WorkQueue(std::uint32_t numWorkers, uint32_t maxSize = 0);
    ~WorkQueue();

    /**
     * @brief Put the work queue into a stopping state. This will prevent new jobs from being queued.
     *
     * @param onQueueEmpty A callback to run when the last task in the queue is completed
     */
    void
    stop(std::function<void()> onQueueEmpty);

    /**
     * @brief A factory function that creates the work queue based on a config.
     *
     * @param config The Clio config to use
     * @return The work queue
     */
    static WorkQueue
    make_WorkQueue(util::Config const& config);

    /**
     * @brief Submit a job to the work queue.
     *
     * The job will be rejected if isWhiteListed is set to false and the current size of the queue reached capacity.
     *
     * @tparam FnType The function object type
     * @param func The function object to queue as a job
     * @param isWhiteListed Whether the queue capacity applies to this job
     * @return true if the job was successfully queued; false otherwise
     */
    template <typename FnType>
    bool
    postCoro(FnType&& func, bool isWhiteListed)
    {
        if (stopping_) {
            LOG(log_.warn()) << "Queue is stopping, rejecting incoming task.";
            return false;
        }

        if (curSize_.get().value() >= maxSize_ && !isWhiteListed) {
            LOG(log_.warn()) << "Queue is full. rejecting job. current size = " << curSize_.get().value()
                             << "; max size = " << maxSize_;
            return false;
        }

        ++curSize_.get();

        // Each time we enqueue a job, we want to post a symmetrical job that will dequeue and run the job at the front
        // of the job queue.
        boost::asio::spawn(
            ioc_,
            [this, func = std::forward<FnType>(func), start = std::chrono::system_clock::now()](auto yield) mutable {
                auto const run = std::chrono::system_clock::now();
                auto const wait = std::chrono::duration_cast<std::chrono::microseconds>(run - start).count();

                ++queued_.get();
                durationUs_.get() += wait;
                LOG(log_.info()) << "WorkQueue wait time = " << wait << " queue size = " << curSize_.get().value();

                func(yield);
                --curSize_.get();
                if (curSize_.get().value() == 0 && stopping_) {
                    auto onTasksComplete = onQueueEmpty_.lock();
                    ASSERT(onTasksComplete->operator bool(), "onTasksComplete must be set when stopping is true.");
                    onTasksComplete->operator()();
                }
            }
        );

        return true;
    }

    /**
     * @brief Generate a report of the work queue state.
     *
     * @return The report as a JSON object.
     */
    boost::json::object
    report() const;

    /**
     * @brief Wait until all the jobs in the queue are finished.
     */
    void
    join();

    /**
     * @brief Get the size of the queue.
     *
     * @return The numver of jobs in the queue.
     */
    size_t
    size() const;
};

}  // namespace rpc
