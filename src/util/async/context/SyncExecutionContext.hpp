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
#include "util/async/context/SystemExecutionContext.hpp"
#include "util/async/context/impl/Cancellation.hpp"

#include <boost/asio/error.hpp>

#include <cstddef>

namespace util::async {
namespace impl {

struct SameThreadContext {
    struct Executor {
        Executor(std::size_t)
        {
        }

        void
        stop() const noexcept
        {
        }

        void
        join() noexcept
        {
        }
    };

    // Note: these types are not actually used but needed for compilation
    struct Timer {};
    struct Strand {
        struct Executor {};
        struct Timer {};
    };

    Executor executor;

    void
    stop() const noexcept
    {
        executor.stop();
    }

    void
    join() noexcept
    {
        executor.join();
    }

    Executor&
    getExecutor()
    {
        return executor;
    }

    [[nodiscard]] Strand
    makeStrand() noexcept  // NOLINT(readability-convert-member-functions-to-static)
    {
        return {};
    }
};

struct SystemContextProvider {
    template <typename CtxType>
    [[nodiscard]] static constexpr auto&
    getContext([[maybe_unused]] CtxType& self) noexcept
    {
        return SystemExecutionContext::instance();
    }
};

}  // namespace impl

/**
 * @brief A synchronous execution context. Runs on the caller thread.
 *
 * This execution context runs the operations on the same thread that requested the operation to run.
 * Each operation must finish before the corresponding `execute` returns an operation object that can immediately be
 * queried for value or error as it's guaranteed to have completed. Timer-based operations are scheduled via
 * SystemExecutionContext, including those that are scheduled from within a strand.
 */
using SyncExecutionContext = BasicExecutionContext<
    impl::SameThreadContext,
    impl::BasicStopSource,
    impl::SyncDispatchStrategy,
    impl::SystemContextProvider>;

}  // namespace util::async
