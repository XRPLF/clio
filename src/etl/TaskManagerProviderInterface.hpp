#pragma once

#include "etl/MonitorInterface.hpp"
#include "etl/TaskManagerInterface.hpp"
#include "util/async/AnyExecutionContext.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace etl {

/**
 * @brief An interface for providing the Task Manager
 */
struct TaskManagerProviderInterface {
    virtual ~TaskManagerProviderInterface() = default;

    /**
     * @brief Make a task manager
     *
     * @param ctx The async context to associate the task manager instance with
     * @param monitor The monitor to notify when ledger is loaded
     * @param startSeq The sequence to start at
     * @param finishSeq The sequence to stop at if specified
     * @return A unique pointer to a TaskManager implementation
     */
    [[nodiscard]] virtual std::unique_ptr<TaskManagerInterface>
    make(
        util::async::AnyExecutionContext ctx,
        std::reference_wrapper<MonitorInterface> monitor,
        uint32_t startSeq,
        std::optional<uint32_t> finishSeq = std::nullopt
    ) = 0;
};

}  // namespace etl
