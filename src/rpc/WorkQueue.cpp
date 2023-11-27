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

#include "rpc/WorkQueue.h"
#include "util/prometheus/Label.h"
#include "util/prometheus/Prometheus.h"
#include <cstdint>

namespace rpc {

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
WorkQueue::join()
{
    ioc_.join();
}

}  // namespace rpc
