#pragma once

#include <chrono>
#include <functional>

namespace tests::common::util {

/**
 * @brief Run a function with a timeout. If the function does not complete within the timeout, the
 * test will fail.
 *
 * @param timeout The timeout duration
 * @param function The function to run
 */
void
callWithTimeout(std::chrono::steady_clock::duration timeout, std::function<void()> function);

}  // namespace tests::common::util
