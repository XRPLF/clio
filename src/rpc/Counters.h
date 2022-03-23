#ifndef RPC_COUNTERS_H
#define RPC_COUNTERS_H

#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <shared_mutex>
#include <string>

namespace RPC {

class Counters
{
private:
    struct MethodInfo
    {
        MethodInfo() = default;

        std::atomic_uint started{0};
        std::atomic_uint finished{0};
        std::atomic_uint errored{0};
        std::atomic_uint forwarded{0};
        std::atomic_uint duration{0};
    };

    void
    initializeCounter(std::string const& method);

    std::shared_mutex mutex_;
    std::unordered_map<std::string, MethodInfo> methodInfo_;

public:
    Counters() = default;

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