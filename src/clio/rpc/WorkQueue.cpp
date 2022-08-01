#include <clio/rpc/WorkQueue.h>

namespace RPC {

WorkQueue::WorkQueue(Application const& app)
{
    auto const& config = app.config();

    if (config.maxQueueSize != 0)
        maxSize_ = config.maxQueueSize;

    auto threads = config.socketWorkers;
    while (threads--)
    {
        threads_.emplace_back([this] { ioc_.run(); });
    }
}

}  // namespace RPC