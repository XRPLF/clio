#include <rpc/Counters.h>

namespace RPC
{

void
Counters::rpcErrored(std::string const& method)
{
    std::unique_lock lk(mutex_);

    MethodInfo& counters = methodInfo_[method];
    counters.started++;
    counters.errored++;
}

void
Counters::rpcComplete(
    std::string const& method,
    std::chrono::microseconds const& rpcDuration)
{
    std::unique_lock lk(mutex_);

    MethodInfo& counters = methodInfo_[method];
    counters.started++;
    counters.finished++;
    counters.duration += rpcDuration;
}

void
Counters::rpcForwarded(std::string const& method)
{
    std::unique_lock lk(mutex_);

    MethodInfo& counters = methodInfo_[method];
    counters.forwarded++;
}

boost::json::object
Counters::report()
{
    std::unique_lock lk(mutex_);
    boost::json::object obj = {};
    
    for (auto const& [method, info] : methodInfo_)
    {
        boost::json::object counters = {};
        counters["started"] = std::to_string(info.started);
        counters["finished"] = std::to_string(info.finished);
        counters["errored"] = std::to_string(info.errored);
        counters["forwarded"] = std::to_string(info.forwarded);
        counters["duration_us"] = std::to_string(info.duration.count());

        obj[method] = std::move(counters);
    }

    return obj;
}

} // namespace RPC