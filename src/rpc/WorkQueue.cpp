#include <rpc/WorkQueue.h>

WorkQueue::WorkQueue(std::uint32_t numWorkers, uint32_t maxSize)
{
    if (maxSize != 0)
        maxSize_ = maxSize;
    while (--numWorkers)
    {
        threads_.emplace_back([this] { ioc_.run(); });
    }
}
