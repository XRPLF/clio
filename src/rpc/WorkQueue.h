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

#include <config/Config.h>
#include <log/Logger.h>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>

#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <thread>

class WorkQueue
{
    // these are cumulative for the lifetime of the process
    std::atomic_uint64_t queued_ = 0;
    std::atomic_uint64_t durationUs_ = 0;

    std::atomic_uint64_t curSize_ = 0;
    uint32_t maxSize_ = std::numeric_limits<uint32_t>::max();

    clio::Logger log_{"RPC"};
    std::vector<std::thread> threads_ = {};
    boost::asio::io_context ioc_;
    std::optional<boost::asio::io_context::work> work_;

public:
    WorkQueue(std::uint32_t numWorkers, uint32_t maxSize = 0);
    ~WorkQueue();

    static WorkQueue
    make_WorkQueue(clio::Config const& config)
    {
        static clio::Logger log{"RPC"};
        auto const serverConfig = config.section("server");
        auto const numThreads = config.valueOr<uint32_t>("workers", std::thread::hardware_concurrency());
        auto const maxQueueSize = serverConfig.valueOr<uint32_t>("max_queue_size", 0);  // 0 is no limit

        log.info() << "Number of workers = " << numThreads << ". Max queue size = " << maxQueueSize;
        return WorkQueue{numThreads, maxQueueSize};
    }

    template <typename F>
    bool
    postCoro(F&& f, bool isWhiteListed)
    {
        if (curSize_ >= maxSize_ && !isWhiteListed)
        {
            log_.warn() << "Queue is full. rejecting job. current size = " << curSize_ << "; max size = " << maxSize_;
            return false;
        }

        ++curSize_;

        // Each time we enqueue a job, we want to post a symmetrical job that will dequeue and run the job at the front
        // of the job queue.
        boost::asio::spawn(
            ioc_, [this, f = std::forward<F>(f), start = std::chrono::system_clock::now()](auto yield) mutable {
                auto const run = std::chrono::system_clock::now();
                auto const wait = std::chrono::duration_cast<std::chrono::microseconds>(run - start).count();

                ++queued_;
                durationUs_ += wait;
                log_.info() << "WorkQueue wait time = " << wait << " queue size = " << curSize_;

                f(yield);
                --curSize_;
            });

        return true;
    }

    boost::json::object
    report() const
    {
        auto obj = boost::json::object{};

        obj["queued"] = queued_;
        obj["queued_duration_us"] = durationUs_;
        obj["current_queue_size"] = curSize_;
        obj["max_queue_size"] = maxSize_;

        return obj;
    }
};
