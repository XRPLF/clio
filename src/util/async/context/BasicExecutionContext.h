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

#include "util/Expected.h"
#include "util/async/Concepts.h"
#include "util/async/Error.h"
#include "util/async/Operation.h"
#include "util/async/Outcome.h"
#include "util/async/context/impl/Cancellation.h"
#include "util/async/context/impl/Execution.h"
#include "util/async/context/impl/Strand.h"
#include "util/async/context/impl/Timer.h"
#include "util/async/context/impl/Utils.h"
#include "util/async/impl/ErrorHandling.h"

#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace util::async {
namespace detail {

struct AsioPoolStrandContext {
    using Executor = boost::asio::strand<boost::asio::thread_pool::executor_type>;
    using Timer = SteadyTimer<Executor>;

    Executor executor;
};

struct AsioPoolContext {
    using Executor = boost::asio::thread_pool;
    using Timer = SteadyTimer<Executor>;
    using Strand = AsioPoolStrandContext;

    Strand
    makeStrand()
    {
        return {boost::asio::make_strand(executor)};
    }

    boost::asio::thread_pool executor;
};

}  // namespace detail

/**
 * @brief A highly configurable execution context.
 *
 * This execution context is used as the base for all specialized execution contexts.
 * Return values are handled by capturing them and returning them packaged as util::Expected.
 * Exceptions may or may not be caught and handled depending on the error strategy. The default behavior is to catch and
 * package them as the error channel of util::Expected.
 */
template <
    typename ContextType,
    typename StopSourceType,
    typename DispatcherType,
    typename TimerContextProvider = detail::SelfContextProvider,
    typename ErrorHandlerType = detail::DefaultErrorHandler>
class BasicExecutionContext {
    ContextType context_;
    friend detail::AssociatedExecutorExtractor;

public:
    using ContextHolderType = ContextType;
    using ExecutorType = typename ContextHolderType::Executor;

    template <typename T>
    using ValueType = util::Expected<T, ExecutionError>;

    using StopSource = StopSourceType;

    using StopToken = typename StopSourceType::Token;

    template <typename T>
    using StoppableOperation = StoppableOperation<ValueType<T>, StopSourceType>;

    template <typename T>
    using Operation = Operation<ValueType<T>>;

    using Strand = detail::
        BasicStrand<BasicExecutionContext, StopSourceType, DispatcherType, TimerContextProvider, ErrorHandlerType>;

    using Timer = typename ContextHolderType::Timer;

    // note: scheduled operations are always stoppable
    template <typename T>
    using ScheduledOperation = ScheduledOperation<BasicExecutionContext, StoppableOperation<T>>;

    /**
     * @brief Create a new execution context with the given number of threads.
     *
     * @param numThreads The number of threads to use for the underlying thread pool
     */
    explicit BasicExecutionContext(std::size_t numThreads = 1) noexcept : context_{numThreads}
    {
    }

    /**
     * @brief Stops and joins the underlying thread pool.
     */
    ~BasicExecutionContext()
    {
        stop();
    }

    BasicExecutionContext(BasicExecutionContext&&) = delete;
    BasicExecutionContext(BasicExecutionContext const&) = delete;

    [[nodiscard]] auto
    scheduleAfter(
        SomeStdDuration auto delay,
        SomeHandlerWith<StopToken> auto&& fn,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    )
    {
        if constexpr (not std::is_same_v<decltype(TimerContextProvider::getContext(*this)), decltype(*this)>) {
            return TimerContextProvider::getContext(*this).scheduleAfter(
                delay, std::forward<decltype(fn)>(fn), timeout
            );
        } else {
            using FnRetType = std::decay_t<decltype(fn(std::declval<StopToken>()))>;
            return ScheduledOperation<FnRetType>(
                detail::extractAssociatedExecutor(*this),
                delay,
                [this, timeout, fn = std::forward<decltype(fn)>(fn)](auto) mutable {
                    return this->execute(
                        [fn = std::forward<decltype(fn)>(fn)](auto stopToken) {
                            if constexpr (std::is_void_v<FnRetType>) {
                                fn(std::move(stopToken));
                            } else {
                                return fn(std::move(stopToken));
                            }
                        },
                        timeout
                    );
                }
            );
        }
    }

    [[nodiscard]] auto
    scheduleAfter(
        SomeStdDuration auto delay,
        SomeHandlerWith<StopToken, bool> auto&& fn,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    )
    {
        if constexpr (not std::is_same_v<decltype(TimerContextProvider::getContext(*this)), decltype(*this)>) {
            return TimerContextProvider::getContext(*this).scheduleAfter(
                delay, std::forward<decltype(fn)>(fn), timeout
            );
        } else {
            using FnRetType = std::decay_t<decltype(fn(std::declval<StopToken>(), true))>;
            return ScheduledOperation<FnRetType>(
                detail::extractAssociatedExecutor(*this),
                delay,
                [this, timeout, fn = std::forward<decltype(fn)>(fn)](auto ec) mutable {
                    return this->execute(
                        [fn = std::forward<decltype(fn)>(fn),
                         isAborted = (ec == boost::asio::error::operation_aborted)](auto stopToken) {
                            if constexpr (std::is_void_v<FnRetType>) {
                                fn(std::move(stopToken), isAborted);
                            } else {
                                return fn(std::move(stopToken), isAborted);
                            }
                        },
                        timeout
                    );
                }
            );
        }
    }

    /**
     * @brief Schedule an operation on the execution context
     *
     * @param fn The block of code to execute with stop token as first arg.
     * @param timeout The optional timeout duration after which the operation will be cancelled
     */
    [[nodiscard]] auto
    execute(SomeHandlerWith<StopToken> auto&& fn, std::optional<std::chrono::milliseconds> timeout = std::nullopt)
    {
        return DispatcherType::dispatch(
            context_,
            detail::outcomeForHandler<StopSourceType>(fn),
            ErrorHandlerType::wrap([this, timeout, fn = std::forward<decltype(fn)>(fn)](
                                       auto& outcome, auto& stopSource, auto stopToken
                                   ) mutable {
                [[maybe_unused]] auto timeoutHandler =
                    detail::getTimeoutHandleIfNeeded(TimerContextProvider::getContext(*this), timeout, stopSource);

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
    execute(SomeHandlerWith<StopToken> auto&& fn, SomeStdDuration auto timeout)
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
     * @param timeout The timeout duration after which the operation will be cancelled
     */
    [[nodiscard]] auto
    execute(SomeHandlerWithoutStopToken auto&& fn)
    {
        return DispatcherType::dispatch(
            context_,
            detail::outcomeForHandler<StopSourceType>(fn),
            ErrorHandlerType::wrap([fn = std::forward<decltype(fn)>(fn)](auto& outcome) mutable {
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

    /**
     * @brief Create a strand for this execution context
     */
    [[nodiscard]] Strand
    makeStrand()
    {
        return Strand(*this, context_.makeStrand());
    }

    /**
     * @brief Stop the execution context as soon as possible
     */
    void
    stop() noexcept
    {
        context_.executor.stop();
    }
};

/**
 * @brief A Boost.Coroutine-based (asio yield_context) execution context.
 *
 * This execution context uses `asio::spawn` to create a coroutine per executed operation.
 * The stop token that is sent to the lambda to execute is @ref detail::YieldContextStopSource::Token
 * and is special in the way that each time your code checks `token.isStopRequested()` the coroutine will
 * be suspended and other work such as timers and/or other operations in the queue will get a chance to run.
 * This makes it possible to have 1 thread in the execution context and still be able to execute operations AND timers
 * at the same time.
 */
using CoroExecutionContext =
    BasicExecutionContext<detail::AsioPoolContext, detail::YieldContextStopSource, detail::SpawnDispatchStrategy>;

/**
 * @brief A asio::thread_pool-based execution context.
 *
 * This execution context uses `asio::post` to dispatch operations to the thread pool.
 * Please note that this execution context can't handle timers and operations at the same time iff you have exactly 1
 * thread in the thread pool.
 */
using PoolExecutionContext =
    BasicExecutionContext<detail::AsioPoolContext, detail::BasicStopSource, detail::PostDispatchStrategy>;

}  // namespace util::async
