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
#include "util/async/impl/Any.hpp"
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
    template <typename CtxType>
        requires(not std::is_same_v<std::decay_t<CtxType>, AnyExecutionContext>)
    /* implicit */ AnyExecutionContext(CtxType&& ctx)
        : pimpl_{std::make_unique<Model<CtxType>>(std::forward<CtxType>(ctx))}
    {
    }

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
        static_assert(not std::is_same_v<RetType, impl::Any>);

        return AnyOperation<RetType>(pimpl_->execute([fn = std::forward<decltype(fn)>(fn)]() -> impl::Any {
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
        static_assert(not std::is_same_v<RetType, impl::Any>);

        return AnyOperation<RetType>(
            pimpl_->execute([fn = std::forward<decltype(fn)>(fn)](auto stopToken) -> impl::Any {
                if constexpr (std::is_void_v<RetType>) {
                    fn(std::move(stopToken));
                    return {};
                } else {
                    return std::make_any<RetType>(fn(std::move(stopToken)));
                }
            })
        );
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
        static_assert(not std::is_same_v<RetType, impl::Any>);

        return AnyOperation<RetType>(pimpl_->execute(
            [fn = std::forward<decltype(fn)>(fn)](auto stopToken) -> impl::Any {
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
        static_assert(not std::is_same_v<RetType, impl::Any>);

        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(delay);
        return AnyOperation<RetType>(pimpl_->scheduleAfter(
            millis,
            [fn = std::forward<decltype(fn)>(fn)](auto stopToken) -> impl::Any {
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
        static_assert(not std::is_same_v<RetType, impl::Any>);

        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(delay);
        return AnyOperation<RetType>(pimpl_->scheduleAfter(
            millis,
            [fn = std::forward<decltype(fn)>(fn)](auto stopToken, auto cancelled) -> impl::Any {
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

private:
    struct Concept {
        virtual ~Concept() = default;

        virtual impl::ErasedOperation
        execute(
            std::function<impl::Any(AnyStopToken)>,
            std::optional<std::chrono::milliseconds> timeout = std::nullopt
        ) = 0;
        virtual impl::ErasedOperation execute(std::function<impl::Any()>) = 0;
        virtual impl::ErasedOperation
            scheduleAfter(std::chrono::milliseconds, std::function<impl::Any(AnyStopToken)>) = 0;
        virtual impl::ErasedOperation
            scheduleAfter(std::chrono::milliseconds, std::function<impl::Any(AnyStopToken, bool)>) = 0;
        virtual AnyStrand
        makeStrand() = 0;
    };

    template <typename CtxType>
    struct Model : Concept {
        std::reference_wrapper<std::decay_t<CtxType>> ctx;

        Model(CtxType& ctx) : ctx{std::ref(ctx)}
        {
        }

        impl::ErasedOperation
        execute(std::function<impl::Any(AnyStopToken)> fn, std::optional<std::chrono::milliseconds> timeout) override
        {
            return ctx.get().execute(std::move(fn), timeout);
        }

        impl::ErasedOperation
        execute(std::function<impl::Any()> fn) override
        {
            return ctx.get().execute(std::move(fn));
        }

        impl::ErasedOperation
        scheduleAfter(std::chrono::milliseconds delay, std::function<impl::Any(AnyStopToken)> fn) override
        {
            return ctx.get().scheduleAfter(delay, std::move(fn));
        }

        impl::ErasedOperation
        scheduleAfter(std::chrono::milliseconds delay, std::function<impl::Any(AnyStopToken, bool)> fn) override
        {
            return ctx.get().scheduleAfter(delay, std::move(fn));
        }

        AnyStrand
        makeStrand() override
        {
            return ctx.get().makeStrand();
        }
    };

private:
    std::unique_ptr<Concept> pimpl_;
};

}  // namespace util::async
