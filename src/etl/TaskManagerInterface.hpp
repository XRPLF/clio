#pragma once

#include <cstddef>

namespace etl {

/**
 * @brief An interface for the Task Manager
 */
struct TaskManagerInterface {
    virtual ~TaskManagerInterface() = default;

    /**
     * @brief Start the task manager with specified settings
     * @param numExtractors The number of extraction tasks
     */
    virtual void
    run(size_t numExtractors) = 0;

    /**
     * @brief Stop the task manager
     */
    virtual void
    stop() = 0;
};

}  // namespace etl
