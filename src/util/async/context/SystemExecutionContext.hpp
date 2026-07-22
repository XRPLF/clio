#pragma once

#include "util/async/context/BasicExecutionContext.hpp"

namespace util::async {

/**
 * @brief A execution context that runs tasks on a system thread pool of 1 thread.
 *
 * This is useful for timers and system tasks that need to be scheduled on a exececution context
 * that otherwise would not be able to support them (e.g. a synchronous execution context).
 */
class SystemExecutionContext {
public:
    /**
     * @brief Get the instance of the system execution context
     *
     * @return Reference to the global system execution context
     */
    [[nodiscard]] static auto&
    instance()
    {
        static util::async::PoolExecutionContext kSystemExecutionContext{};
        return kSystemExecutionContext;
    }
};

}  // namespace util::async
