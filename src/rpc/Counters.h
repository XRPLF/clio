#ifndef RPC_COUNTERS_H
#define RPC_COUNTERS_H

#include <chrono>
#include <cstdint>
#include <string>
#include <mutex>
#include <boost/json.hpp>

namespace RPC
{

class Counters
{
private:
    struct MethodInfo
    {
        MethodInfo() = default;

        std::uint32_t started{0};
        std::uint32_t finished{0};
        std::uint32_t errored{0};
        std::uint32_t forwarded{0};
        std::chrono::microseconds duration{0};
    };

    std::mutex mutex_;
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

} // namespace RPCs

#endif // RPC_COUNTERS_H