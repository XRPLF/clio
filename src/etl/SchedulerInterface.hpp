#pragma once

#include "etl/Models.hpp"

#include <optional>

namespace etl {

/**
 * @brief The interface of a scheduler for the extraction process
 */
struct SchedulerInterface {
    virtual ~SchedulerInterface() = default;

    /**
     * @brief Attempt to obtain the next task
     * @return A task if one exists; std::nullopt otherwise
     */
    [[nodiscard]] virtual std::optional<model::Task>
    next() = 0;
};

}  // namespace etl
