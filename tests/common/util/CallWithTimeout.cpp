#include "util/CallWithTimeout.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <future>
#include <thread>

namespace tests::common::util {

void
callWithTimeout(std::chrono::steady_clock::duration timeout, std::function<void()> function)
{
    std::promise<void> promise;
    auto future = promise.get_future();
    std::thread t([&promise, &function] {
        function();
        promise.set_value();
    });
    if (future.wait_for(timeout) == std::future_status::timeout) {
        FAIL() << "Timeout "
               << std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()
               << "ms exceeded";
    }
    t.join();
}

}  // namespace tests::common::util
