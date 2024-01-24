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

#include "util/async/Concepts.h"
#include "util/async/context/BasicExecutionContext.h"
#include "util/async/context/SystemExecutionContext.h"
#include "util/async/context/impl/Cancellation.h"

#include <boost/asio/error.hpp>

#include <cstddef>

namespace util::async {
namespace detail {

struct SameThreadContext {
    struct Executor {
        Executor(std::size_t)
        {
        }

        void
        stop() noexcept
        {
        }
    };
    struct Timer {};  // note: not actually used but needed for compilation
    struct Strand {
        using Executor = SameThreadContext::Executor;
        using Timer = SameThreadContext::Timer;
    };

    Executor executor;

    [[nodiscard]] Strand
    makeStrand() noexcept  // NOLINT(readability-convert-member-functions-to-static)
    {
        return {};
    }
};

struct SystemContextProvider {
    template <SomeExecutionContextOrStrand CtxType>
    [[nodiscard]] static constexpr auto&
    getContext([[maybe_unused]] CtxType& self) noexcept
    {
        return SystemExecutionContext::instance();
    }
};

}  // namespace detail

/**
 * @brief A synchronous execution context. Runs on the caller thread.
 *
 * This execution context runs the operations on the same thread that requested the operation to run.
 * Each operation must finish before the corresponding `execute` returns an operation object that can immediately be
 * queried for value or error as it's guaranteed to have completed. Timer-based operations are scheduled via
 * SystemExecutionContext.
 */
using SyncExecutionContext = BasicExecutionContext<
    detail::SameThreadContext,
    detail::BasicStopSource,
    detail::SyncDispatchStrategy,
    detail::SystemContextProvider>;

}  // namespace util::async
