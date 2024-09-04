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

#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <boost/json/object.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>
#include <utility>

namespace rpc {

void
WorkQueue::OneTimeCallable::setCallable(std::function<void()> func)
{
    func_ = func;
}

void
WorkQueue::OneTimeCallable::operator()()
{
    if (not called_) {
        func_();
        called_ = true;
    }
}
WorkQueue::OneTimeCallable::operator bool() const
{
    return func_.operator bool();
}

WorkQueue::WorkQueue(std::uint32_t numWorkers, uint32_t maxSize)
    : queued_{PrometheusService::counterInt(
          "work_queue_queued_total_number",
          util::prometheus::Labels(),
          "The total number of tasks queued for processing"
      )}
    , durationUs_{PrometheusService::counterInt(
          "work_queue_cumulitive_tasks_duration_us",
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

WorkQueue::~WorkQueue()
{
    join();
}

void
WorkQueue::stop(std::function<void()> onQueueEmpty)
{
    auto handler = onQueueEmpty_.lock();
    handler->setCallable(std::move(onQueueEmpty));
    stopping_ = true;
    if (size() == 0) {
        handler->operator()();
    }
}

WorkQueue
WorkQueue::make_WorkQueue(util::config::ClioConfigDefinition const& config)
{
    static util::Logger const log{"RPC"};
    auto const serverConfig = config.getObject("server");
    auto const numThreads = config.getValue("workers").asIntType<uint32_t>();
    auto const maxQueueSize = serverConfig.getValue("max_queue_size").asIntType<uint32_t>();  // 0 is no limit

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

void
WorkQueue::join()
{
    ioc_.join();
}

size_t
WorkQueue::size() const
{
    return curSize_.get().value();
}

}  // namespace rpc
