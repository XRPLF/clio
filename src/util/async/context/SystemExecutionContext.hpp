//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/async/context/BasicExecutionContext.hpp"

namespace util::async {

/**
 * @brief A execution context that runs tasks on a system thread pool of 1 thread.
 *
 * This is useful for timers and system tasks that need to be scheduled on a exececution context that otherwise would
 * not be able to support them (e.g. a synchronous execution context).
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
        static util::async::PoolExecutionContext systemExecutionContext{};
        return systemExecutionContext;
    }
};

}  // namespace util::async
