//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

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
