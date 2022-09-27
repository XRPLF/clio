#include <rpc/WorkQueue.h>

WorkQueue::WorkQueue(std::uint32_t numWorkers, std::optional<uint32_t> maxSize)
    : maxSize_{maxSize.value_or(std::numeric_limits<uint32_t>::max())}
{
    while (--numWorkers)
        threads_.emplace_back([this] { ioc_.run(); });
}

boost::json::object
WorkQueue::report() const
{
    boost::json::object obj;
    obj["queued"] = queued_;
    obj["queued_duration_us"] = durationUs_;
    obj["current_queue_size"] = curSize_;
    obj["max_queue_size"] = maxSize_;
    return obj;
}
