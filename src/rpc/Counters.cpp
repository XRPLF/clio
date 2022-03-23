#include <rpc/Counters.h>
#include <rpc/RPC.h>

namespace RPC {

void
Counters::initializeCounter(std::string const& method)
{
    std::shared_lock lk(mutex_);
    if (methodInfo_.count(method) == 0)
    {
        lk.unlock();
        std::unique_lock ulk(mutex_);

        // This calls the default constructor for methodInfo of the method.
        methodInfo_[method];
    }
}

void
Counters::rpcErrored(std::string const& method)
{
    if (!validHandler(method))
        return;

    initializeCounter(method);

    std::shared_lock lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    counters.started++;
    counters.errored++;
}

void
Counters::rpcComplete(
    std::string const& method,
    std::chrono::microseconds const& rpcDuration)
{
    if (!validHandler(method))
        return;

    initializeCounter(method);

    std::shared_lock lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    counters.started++;
    counters.finished++;
    counters.duration += rpcDuration.count();
}

void
Counters::rpcForwarded(std::string const& method)
{
    if (!validHandler(method))
        return;

    initializeCounter(method);

    std::shared_lock lk(mutex_);
    MethodInfo& counters = methodInfo_[method];
    counters.forwarded++;
}

boost::json::object
Counters::report()
{
    std::shared_lock lk(mutex_);
    boost::json::object obj = {};

    for (auto const& [method, info] : methodInfo_)
    {
        boost::json::object counters = {};
        counters["started"] = std::to_string(info.started);
        counters["finished"] = std::to_string(info.finished);
        counters["errored"] = std::to_string(info.errored);
        counters["forwarded"] = std::to_string(info.forwarded);
        counters["duration_us"] = std::to_string(info.duration);

        obj[method] = std::move(counters);
    }

    return obj;
}

}  // namespace RPC