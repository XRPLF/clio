#ifndef RPC_COUNTERS_H
#define RPC_COUNTERS_H

#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <rpc/WorkQueue.h>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace RPC {

class Counters
{
private:
    struct MethodInfo
    {
        MethodInfo() = default;

        std::atomic_uint64_t started{0};
        std::atomic_uint64_t finished{0};
        std::atomic_uint64_t errored{0};
        std::atomic_uint64_t forwarded{0};
        std::atomic_uint64_t duration{0};
    };

    void
    initializeCounter(std::string const& method);

    std::shared_mutex mutex_;
    std::unordered_map<std::string, MethodInfo> methodInfo_;

    std::reference_wrapper<const WorkQueue> workQueue_;

public:
    Counters(WorkQueue const& wq) : workQueue_(std::cref(wq)){};

    void
    rpcErrored(std::string const& method);

    void
    rpcComplete(
        std::string const& method,
        std::chrono::microseconds const& rpcDuration);

    void
    rpcForwarded(std::string const& method);

    boost::json::object
    report();
};

}  // namespace RPC

#endif  // RPC_COUNTERS_H
