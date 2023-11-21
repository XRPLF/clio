//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <util/Expected.h>
#include <util/async/Concepts.h>
#include <util/async/Error.h>
#include <util/async/Operation.h>
#include <util/async/context/SystemExecutionContext.h>
#include <util/async/context/impl/Cancellation.h>
#include <util/async/context/impl/Execution.h>

#include <exception>

namespace util::async {

/**
 * @brief A synchronous execution context. Runs on the caller thread.
 *
 * This execution context runs the operations on the same thread that requested the operation to run.
 * Each operation must finish before the corresponding `execute` returns an operation object that can immediately be
 * queried for value or error as it's guaranteed to have completed.
 */
template <typename ErrorHandlingStrategy = detail::DefaultErrorHandler>
class BasicSyncExecutionContext {
    detail::SyncDispatcher dispatcher_;

public:
    template <typename T>
    using ValueType = util::Expected<T, ExecutionContextException>;

    using StopSource = detail::BasicStopSource;

    using StopToken = typename StopSource::Token;

    template <typename T>
    using CancellableOperation = CancellableOperation<ValueType<T>, StopSource>;

    template <typename T>
    using Operation = Operation<ValueType<T>>;

    // numThreads is never used, but we keep it for compatibility with other execution contexts
    BasicSyncExecutionContext([[maybe_unused]] std::size_t numThreads = 1)
    {
    }
    BasicSyncExecutionContext(BasicSyncExecutionContext&&) = delete;
    BasicSyncExecutionContext(BasicSyncExecutionContext const&) = delete;

    /**
     * @brief Schedule a timer on the global system execution context.
     */
    [[nodiscard]] auto
    scheduleAfter(SomeStdDuration auto delay, SomeHandlerWithSignature<void()> auto&& fn) noexcept
    {
        return SystemExecutionContext::instance().scheduleAfter(delay, std::forward<decltype(fn)>(fn));
    }

    /**
     * @brief Schedule a timer on the global system execution context. Callback receives a cancellation flag.
     */
    [[nodiscard]] auto
    scheduleAfter(SomeStdDuration auto delay, SomeHandlerWithSignature<void(bool)> auto&& fn) noexcept
    {
        return SystemExecutionContext::instance().scheduleAfter(delay, std::forward<decltype(fn)>(fn));
    }

    /**
     * @brief Schedule an operation on the execution context
     *
     * @param fn The block of code to execute with stop token as first arg.
     * @param timeout The optional timeout duration after which the operation will be cancelled
     */
    [[nodiscard]] auto
    execute(
        SomeHandlerWithStopToken auto&& fn,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    ) noexcept
    {
        return dispatcher_.dispatch(
            detail::outcomeForHandler<StopSource>(fn),
            ErrorHandlingStrategy::wrap([timeout, fn = std::forward<decltype(fn)>(fn)](
                                            auto& outcome, auto& stopSource, auto stopToken
                                        ) mutable {
                auto timeoutHandler =
                    detail::getTimoutHandleIfNeeded(SystemExecutionContext::instance(), timeout, stopSource);

                using FnRetType = std::decay_t<decltype(fn(std::declval<StopToken>()))>;
                if constexpr (std::is_void_v<FnRetType>) {
                    fn(std::move(stopToken));
                    outcome.setValue();
                } else {
                    outcome.setValue(fn(std::move(stopToken)));
                }
            })
        );
    }

    /**
     * @brief Schedule an operation on the execution context
     *
     * @param fn The block of code to execute with stop token as first arg. Signature is `Type(auto stopToken)` where
     * `Type` is the return type.
     * @param timeout The timeout duration after which the operation will be cancelled
     */
    [[nodiscard]] auto
    execute(SomeHandlerWithStopToken auto&& fn, SomeStdDuration auto timeout) noexcept
    {
        return execute(
            std::forward<decltype(fn)>(fn),
            std::make_optional(std::chrono::duration_cast<std::chrono::milliseconds>(timeout))
        );
    }

    /**
     * @brief Schedule an operation on the execution context
     *
     * @param fn The block of code to execute. Signature is `Type()` where `Type` is the return type.
     */
    [[nodiscard]] auto
    execute(SomeHandlerWithoutStopToken auto&& fn) noexcept
    {
        return dispatcher_.dispatch(
            detail::outcomeForHandler<StopSource>(fn),
            ErrorHandlingStrategy::wrap([fn = std::forward<decltype(fn)>(fn)](auto& outcome) mutable {
                using FnRetType = std::decay_t<decltype(fn())>;
                if constexpr (std::is_void_v<FnRetType>) {
                    fn();
                    outcome.setValue();
                } else {
                    outcome.setValue(fn());
                }
            })
        );
    }

    // TODO: it could be possible to reuse BasicStrand here if the context is hidden
    struct Strand {
        Strand() = default;
        ~Strand() = default;
        Strand(Strand&&) = default;
        Strand(Strand const&) = delete;

        [[nodiscard]] auto
        execute(
            SomeHandlerWithStopToken auto&& fn,
            std::optional<std::chrono::milliseconds> timeout = std::nullopt
        ) noexcept
        {
            return dispatcher_.dispatch(
                detail::outcomeForHandler<StopSource>(fn),
                ErrorHandlingStrategy::wrap([timeout, fn = std::forward<decltype(fn)>(fn)](
                                                auto& outcome, auto& stopSource, auto stopToken
                                            ) mutable {
                    auto timeoutHandler =
                        detail::getTimoutHandleIfNeeded(SystemExecutionContext::instance(), timeout, stopSource);

                    using FnRetType = std::decay_t<decltype(fn(std::declval<StopToken>()))>;
                    if constexpr (std::is_void_v<FnRetType>) {
                        fn(std::move(stopToken));
                        outcome.setValue();
                    } else {
                        outcome.setValue(fn(std::move(stopToken)));
                    }
                })
            );
        }

        [[nodiscard]] auto
        execute(SomeHandlerWithStopToken auto&& fn, SomeStdDuration auto timeout) noexcept
        {
            return execute(
                std::forward<decltype(fn)>(fn),
                std::make_optional(std::chrono::duration_cast<std::chrono::milliseconds>(timeout))
            );
        }

        [[nodiscard]] auto
        execute(SomeHandlerWithoutStopToken auto&& fn) noexcept
        {
            return dispatcher_.dispatch(
                detail::outcomeForHandler<StopSource>(fn),
                ErrorHandlingStrategy::wrap([fn = std::forward<decltype(fn)>(fn)](auto& outcome) mutable {
                    using FnRetType = std::decay_t<decltype(fn())>;
                    if constexpr (std::is_void_v<FnRetType>) {
                        fn();
                        outcome.setValue();
                    } else {
                        outcome.setValue(fn());
                    }
                })
            );
        }

    private:
        detail::SyncDispatcher dispatcher_;
    };

    /**
     * @brief Create a strand for this execution context
     */
    [[nodiscard]] Strand
    makeStrand()
    {
        return Strand();
    }

    /**
     * @brief Does nothing for this execution context
     */
    void
    stop()
    {
        // nop
    }
};

/**
 * @brief A synchronous execution context. Runs on the caller thread.
 */
using SyncExecutionContext = BasicSyncExecutionContext<>;

}  // namespace util::async
