#pragma once

#include <chrono>
#include <type_traits>
#include <utility>

namespace util {

/**
 * @brief Profiler function to measure the time a function execution consumes.
 *
 * @tparam U The duration measurement to use; defaults to milliseconds
 * @tparam FnType The type of the function object
 * @param func Any function object
 * @return If the function object has a return value, the result of the function call and the
 * elapsed time(ms) is returned as a pair
 * @return Only return the elapsed time if passed function object does not have a return value
 */
template <typename U = std::chrono::milliseconds, typename FnType>
[[nodiscard]] auto
timed(FnType&& func)
{
    auto start = std::chrono::system_clock::now();

    if constexpr (std::is_same_v<decltype(func()), void>) {
        func();
        return std::chrono::duration_cast<U>(std::chrono::system_clock::now() - start).count();
    } else {
        auto ret = func();
        auto elapsed =
            std::chrono::duration_cast<U>(std::chrono::system_clock::now() - start).count();
        return std::make_pair(std::move(ret), std::move(elapsed));
    }
}

}  // namespace util
