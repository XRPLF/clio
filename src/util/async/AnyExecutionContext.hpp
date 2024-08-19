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

#include "util/async/AnyOperation.hpp"
#include "util/async/AnyStopToken.hpp"
#include "util/async/AnyStrand.hpp"
#include "util/async/Concepts.hpp"
#include "util/async/impl/ErasedOperation.hpp"

#include <any>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace util::async {

/**
 * @brief A type-erased execution context
 */
class AnyExecutionContext {
public:
    /**
     * @brief Construct a new type-erased Execution Context object
     *
     * @note Stores the Execution Context by reference.
     *
     * @tparam CtxType The type of the execution context to wrap
     * @param ctx The execution context to wrap
     */
    template <NotSameAs<AnyExecutionContext> CtxType>
    /* implicit */
    AnyExecutionContext(CtxType& ctx) : pimpl_{std::make_shared<Model<CtxType&>>(ctx)}
    {
    }

    /**
     * @brief Construct a new type-erased Execution Context object
     *
     * @note Stores the Execution Context by moving it into the AnyExecutionContext.
     *
     * @tparam CtxType The type of the execution context to wrap
     * @param ctx The execution context to wrap
     */
    template <RValueNotSameAs<AnyExecutionContext> CtxType>
    /* implicit */
    AnyExecutionContext(CtxType&& ctx) : pimpl_{std::make_shared<Model<CtxType>>(std::forward<CtxType>(ctx))}
    {
    }

    AnyExecutionContext(AnyExecutionContext const&) = default;
    AnyExecutionContext(AnyExecutionContext&&) = default;
    AnyExecutionContext&
    operator=(AnyExecutionContext const&) = default;
    AnyExecutionContext&
    operator=(AnyExecutionContext&&) = default;
    ~AnyExecutionContext() = default;

    /**
     * @brief Execute a function on the execution context
     *
     * @param fn The function to execute
     * @returns A unstoppable operation that can be used to wait for the result
     */
    [[nodiscard]] auto
    execute(SomeHandlerWithoutStopToken auto&& fn)
    {
        using RetType = std::decay_t<decltype(fn())>;
        static_assert(not std::is_same_v<RetType, std::any>);

        return AnyOperation<RetType>(pimpl_->execute([fn = std::forward<decltype(fn)>(fn)]() -> std::any {
            if constexpr (std::is_void_v<RetType>) {
                fn();
                return {};
            } else {
                return std::make_any<RetType>(fn());
            }
        }));
    }

    /**
     * @brief Execute a function on the execution context
     *
     * @param fn The function to execute
     * @returns A stoppable operation that can be used to wait for the result
     *
     * @note The function is expected to take a stop token
     */
    [[nodiscard]] auto
    execute(SomeHandlerWith<AnyStopToken> auto&& fn)
    {
        using RetType = std::decay_t<decltype(fn(std::declval<AnyStopToken>()))>;
        static_assert(not std::is_same_v<RetType, std::any>);

        return AnyOperation<RetType>(pimpl_->execute([fn = std::forward<decltype(fn)>(fn)](auto stopToken) -> std::any {
            if constexpr (std::is_void_v<RetType>) {
                fn(std::move(stopToken));
                return {};
            } else {
                return std::make_any<RetType>(fn(std::move(stopToken)));
            }
        }));
    }

    /**
     * @brief Execute a function with a timeout
     *
     * @param fn The function to execute
     * @param timeout The timeout after which the function should be cancelled
     * @returns A stoppable operation that can be used to wait for the result
     *
     * @note The function is expected to take a stop token
     */
    [[nodiscard]] auto
    execute(SomeHandlerWith<AnyStopToken> auto&& fn, SomeStdDuration auto timeout)
    {
        using RetType = std::decay_t<decltype(fn(std::declval<AnyStopToken>()))>;
        static_assert(not std::is_same_v<RetType, std::any>);

        return AnyOperation<RetType>(pimpl_->execute(
            [fn = std::forward<decltype(fn)>(fn)](auto stopToken) -> std::any {
                if constexpr (std::is_void_v<RetType>) {
                    fn(std::move(stopToken));
                    return {};
                } else {
                    return std::make_any<RetType>(fn(std::move(stopToken)));
                }
            },
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout)
        ));
    }

    /**
     * @brief Schedule a function for execution
     *
     * @param delay The delay after which the function should be executed
     * @param fn The function to execute
     * @returns A stoppable operation that can be used to wait for the result
     *
     * @note The function is expected to take a stop token
     */
    [[nodiscard]] auto
    scheduleAfter(SomeStdDuration auto delay, SomeHandlerWith<AnyStopToken> auto&& fn)
    {
        using RetType = std::decay_t<decltype(fn(std::declval<AnyStopToken>()))>;
        static_assert(not std::is_same_v<RetType, std::any>);

        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(delay);
        return AnyOperation<RetType>(pimpl_->scheduleAfter(
            millis,
            [fn = std::forward<decltype(fn)>(fn)](auto stopToken) -> std::any {
                if constexpr (std::is_void_v<RetType>) {
                    fn(std::move(stopToken));
                    return {};
                } else {
                    return std::make_any<RetType>(fn(std::move(stopToken)));
                }
            }
        ));
    }

    /**
     * @brief Schedule a function for execution
     *
     * @param delay The delay after which the function should be executed
     * @param fn The function to execute
     * @returns A stoppable operation that can be used to wait for the result
     *
     * @note The function is expected to take a stop token and a boolean representing whether the scheduled operation
     * got cancelled
     */
    [[nodiscard]] auto
    scheduleAfter(SomeStdDuration auto delay, SomeHandlerWith<AnyStopToken, bool> auto&& fn)
    {
        using RetType = std::decay_t<decltype(fn(std::declval<AnyStopToken>(), true))>;
        static_assert(not std::is_same_v<RetType, std::any>);

        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(delay);
        return AnyOperation<RetType>(pimpl_->scheduleAfter(
            millis,
            [fn = std::forward<decltype(fn)>(fn)](auto stopToken, auto cancelled) -> std::any {
                if constexpr (std::is_void_v<RetType>) {
                    fn(std::move(stopToken), cancelled);
                    return {};
                } else {
                    return std::make_any<RetType>(fn(std::move(stopToken), cancelled));
                }
            }
        ));
    }

    /**
     * @brief Make a strand for this execution context
     *
     * @return A strand for this execution context
     *
     * @note The strand can be used similarly to the execution context and guarantees serial execution of all submitted
     * operations
     */
    [[nodiscard]] auto
    makeStrand()
    {
        return pimpl_->makeStrand();
    }

    /**
     * @brief Stop the execution context
     */
    void
    stop() const
    {
        pimpl_->stop();
    }

    /**
     * @brief Join the execution context
     */
    void
    join()
    {
        pimpl_->join();
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        virtual impl::ErasedOperation
        execute(
            std::function<std::any(AnyStopToken)>,
            std::optional<std::chrono::milliseconds> timeout = std::nullopt
        ) = 0;
        virtual impl::ErasedOperation execute(std::function<std::any()>) = 0;
        virtual impl::ErasedOperation
            scheduleAfter(std::chrono::milliseconds, std::function<std::any(AnyStopToken)>) = 0;
        virtual impl::ErasedOperation
            scheduleAfter(std::chrono::milliseconds, std::function<std::any(AnyStopToken, bool)>) = 0;
        virtual AnyStrand
        makeStrand() = 0;
        virtual void
        stop() const = 0;
        virtual void
        join() = 0;
    };

    template <typename CtxType>
    struct Model : Concept {
        CtxType ctx;

        template <typename Type>
        Model(Type&& ctx) : ctx(std::forward<Type>(ctx))
        {
        }

        impl::ErasedOperation
        execute(std::function<std::any(AnyStopToken)> fn, std::optional<std::chrono::milliseconds> timeout) override
        {
            return ctx.execute(std::move(fn), timeout);
        }

        impl::ErasedOperation
        execute(std::function<std::any()> fn) override
        {
            return ctx.execute(std::move(fn));
        }

        impl::ErasedOperation
        scheduleAfter(std::chrono::milliseconds delay, std::function<std::any(AnyStopToken)> fn) override
        {
            return ctx.scheduleAfter(delay, std::move(fn));
        }

        impl::ErasedOperation
        scheduleAfter(std::chrono::milliseconds delay, std::function<std::any(AnyStopToken, bool)> fn) override
        {
            return ctx.scheduleAfter(delay, std::move(fn));
        }

        AnyStrand
        makeStrand() override
        {
            return ctx.makeStrand();
        }

        void
        stop() const override
        {
            ctx.stop();
        }

        void
        join() override
        {
            ctx.join();
        }
    };

private:
    std::shared_ptr<Concept> pimpl_;
};

}  // namespace util::async
