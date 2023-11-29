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

#include "util/Expected.h"
#include "util/async/Concepts.h"
#include "util/async/Error.h"
#include "util/async/Operation.h"
#include "util/async/context/impl/Cancellation.h"
#include "util/async/context/impl/Execution.h"
#include "util/async/context/impl/Strand.h"
#include "util/async/context/impl/Timer.h"
#include "util/async/impl/ErrorHandling.h"

#include <boost/asio.hpp>

#include <chrono>
#include <exception>

namespace util::async {

/**
 * @brief A basic, dispatch-agnostic execution context.
 *
 * Both return values and exceptions are handled by capturing them and returning them packaged as util::Expected.
 */
template <
    SomeStopSource StopSourceType,
    template <typename>
    typename DispatchStrategy,
    typename ErrorHandlingStrategy = detail::DefaultErrorHandler>
class BasicExecutionContext {
    using ExecutorType = boost::asio::thread_pool;

    ExecutorType ioc_;
    DispatchStrategy<ExecutorType&> dispatcher_;

public:
    template <typename T>
    using ValueType = util::Expected<T, ExecutionContextException>;

    using StopSource = StopSourceType;

    using StopToken = typename StopSourceType::Token;

    template <typename T>
    using CancellableOperation = CancellableOperation<ValueType<T>, StopSourceType>;

    template <typename T>
    using Operation = Operation<ValueType<T>>;

    using Strand = detail::BasicStrand<BasicExecutionContext, DispatchStrategy, ErrorHandlingStrategy>;
    friend Strand;

    using Timer = detail::SteadyTimer<BasicExecutionContext>;
    friend Timer;

    /**
     * @brief Create a new execution context with the given number of threads.
     *
     * @param numThreads The number of threads to use for the underlying thread pool
     */
    BasicExecutionContext(std::size_t numThreads = 1) noexcept : ioc_{numThreads}, dispatcher_{ioc_}
    {
    }

    /**
     * @brief Stops and joins the underlying thread pool.
     */
    ~BasicExecutionContext()
    {
        stop();
        ioc_.join();
    }

    BasicExecutionContext(BasicExecutionContext&&) = delete;
    BasicExecutionContext(BasicExecutionContext const&) = delete;

    /**
     * @brief Schedule a timer on the global system execution context.
     */
    [[nodiscard]] Timer
    scheduleAfter(SomeStdDuration auto delay, SomeHandlerWithSignature<void()> auto&& fn) noexcept
    {
        return Timer(*this, delay, [fn = std::forward<decltype(fn)>(fn)](auto) mutable {
            try {
                fn();
            } catch (...) {
                throw std::current_exception();
            }
        });
    }

    /**
     * @brief Schedule a timer on the global system execution context. Callback receives a cancellation flag.
     */
    [[nodiscard]] Timer
    scheduleAfter(SomeStdDuration auto delay, SomeHandlerWithSignature<void(bool)> auto&& fn) noexcept
    {
        return Timer(*this, delay, [fn = std::forward<decltype(fn)>(fn)](auto ec) mutable {
            try {
                fn(ec == boost::asio::error::operation_aborted);
            } catch (...) {
                throw std::current_exception();
            }
        });
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
            detail::outcomeForHandler<StopSourceType>(fn),
            ErrorHandlingStrategy::wrap([this, timeout, fn = std::forward<decltype(fn)>(fn)](
                                            auto& outcome, auto& stopSource, auto stopToken
                                        ) mutable {
                auto timeoutHandler = detail::getTimoutHandleIfNeeded(*this, timeout, stopSource);

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
     * @param timeout The timeout duration after which the operation will be cancelled
     */
    [[nodiscard]] auto
    execute(SomeHandlerWithoutStopToken auto&& fn) noexcept
    {
        return dispatcher_.dispatch(
            detail::outcomeForHandler<StopSourceType>(fn),
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

    /**
     * @brief Create a strand for this execution context
     */
    [[nodiscard]] Strand
    makeStrand()
    {
        return Strand(*this);
    }

    /**
     * @brief Stop the execution context as soon as possible
     */
    void
    stop()
    {
        ioc_.stop();
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
using CoroExecutionContext = BasicExecutionContext<detail::YieldContextStopSource, detail::CoroDispatcher>;

/**
 * @brief A asio::thread_pool-based execution context.
 *
 * This execution context uses `asio::post` to dispatch operations to the thread pool.
 * Please note that this execution context can't handle timers and operations at the same time iff you have exactly 1
 * thread in the thread pool.
 */
using PoolExecutionContext = BasicExecutionContext<detail::BasicStopSource, detail::PoolDispatcher>;

}  // namespace util::async
