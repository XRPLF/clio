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

#include <util/config/Config.h>
#include <util/log/Logger.h>
#include <util/prometheus/Prometheus.h>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>

#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <thread>

namespace rpc {

/**
 * @brief An asynchronous, thread-safe queue for RPC requests.
 */
class WorkQueue
{
    // these are cumulative for the lifetime of the process
    util::prometheus::CounterInt& queued_;
    util::prometheus::CounterInt& durationUs_;

    util::prometheus::GaugeInt& curSize_;
    uint32_t maxSize_ = std::numeric_limits<uint32_t>::max();

    util::Logger log_{"RPC"};
    boost::asio::thread_pool ioc_;

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
     * @brief A factory function that creates the work queue based on a config.
     *
     * @param config The Clio config to use
     */
    static WorkQueue
    make_WorkQueue(util::Config const& config)
    {
        static util::Logger const log{"RPC"};
        auto const serverConfig = config.section("server");
        auto const numThreads = config.valueOr<uint32_t>("workers", std::thread::hardware_concurrency());
        auto const maxQueueSize = serverConfig.valueOr<uint32_t>("max_queue_size", 0);  // 0 is no limit

        LOG(log.info()) << "Number of workers = " << numThreads << ". Max queue size = " << maxQueueSize;
        return WorkQueue{numThreads, maxQueueSize};
    }

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
        if (curSize_.value() >= maxSize_ && !isWhiteListed)
        {
            LOG(log_.warn()) << "Queue is full. rejecting job. current size = " << curSize_.value()
                             << "; max size = " << maxSize_;
            return false;
        }

        ++curSize_;

        // Each time we enqueue a job, we want to post a symmetrical job that will dequeue and run the job at the front
        // of the job queue.
        boost::asio::spawn(
            ioc_,
            [this, func = std::forward<FnType>(func), start = std::chrono::system_clock::now()](auto yield) mutable {
                auto const run = std::chrono::system_clock::now();
                auto const wait = std::chrono::duration_cast<std::chrono::microseconds>(run - start).count();

                ++queued_;
                durationUs_ += wait;
                LOG(log_.info()) << "WorkQueue wait time = " << wait << " queue size = " << curSize_.value();

                func(yield);
                --curSize_;
            });

        return true;
    }

    /**
     * @brief Generate a report of the work queue state.
     *
     * @return The report as a JSON object.
     */
    boost::json::object
    report() const
    {
        auto obj = boost::json::object{};

        obj["queued"] = queued_.value();
        obj["queued_duration_us"] = durationUs_.value();
        obj["current_queue_size"] = curSize_.value();
        obj["max_queue_size"] = maxSize_;

        return obj;
    }

    /**
     * @brief Wait untill all the jobs in the queue are finished.
     */
    void
    join();
};

}  // namespace rpc
