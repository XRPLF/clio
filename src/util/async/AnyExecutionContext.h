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

#include <fmt/core.h>
#include <fmt/std.h>
#include <util/Expected.h>
#include <util/async/AnyOperation.h>
#include <util/async/AnyStrand.h>
#include <util/async/AnyTimer.h>
#include <util/async/Concepts.h>
#include <util/async/Error.h>
#include <util/async/impl/Any.h>
#include <util/async/impl/ErasedOperation.h>

#include <any>
#include <chrono>
#include <exception>

namespace util::async {

class AnyExecutionContext {
public:
    template <typename CtxType>
        requires(not std::is_same_v<std::decay_t<CtxType>, AnyExecutionContext>)
    /* implicit */ AnyExecutionContext(CtxType&& ctx)
        : pimpl_{std::make_unique<Model<CtxType>>(std::forward<CtxType>(ctx))}
    {
    }

    ~AnyExecutionContext() = default;

    [[nodiscard]] auto
    execute(SomeHandlerWithoutStopToken auto&& fn) noexcept
    {
        using RetType = std::decay_t<decltype(fn())>;
        static_assert(not std::is_same_v<RetType, detail::Any>);

        return AnyOperation<RetType>(pimpl_->execute([fn = std::forward<decltype(fn)>(fn)]() -> detail::Any {
            if constexpr (std::is_void_v<RetType>) {
                fn();
                return {};
            } else {
                return std::make_any<RetType>(fn());
            }
        }));
    }

    [[nodiscard]] auto
    execute(auto&& fn) noexcept
    {
        using RetType = std::decay_t<decltype(fn(std::declval<AnyStopToken>()))>;
        static_assert(not std::is_same_v<RetType, detail::Any>);

        return AnyOperation<RetType>(
            pimpl_->execute([fn = std::forward<decltype(fn)>(fn)](auto stopToken) -> detail::Any {
                if constexpr (std::is_void_v<RetType>) {
                    fn(std::move(stopToken));
                    return {};
                } else {
                    return std::make_any<RetType>(fn(std::move(stopToken)));
                }
            })
        );
    }

    [[nodiscard]] auto
    execute(auto&& fn, SomeStdDuration auto timeout) noexcept
    {
        using RetType = std::decay_t<decltype(fn(std::declval<AnyStopToken>()))>;
        static_assert(not std::is_same_v<RetType, detail::Any>);

        return AnyOperation<RetType>(pimpl_->execute(
            [fn = std::forward<decltype(fn)>(fn)](auto stopToken) -> detail::Any {
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

    [[nodiscard]] auto
    scheduleAfter(SomeStdDuration auto delay, SomeHandlerWithSignature<void()> auto&& fn) noexcept
    {
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(delay);
        return pimpl_->scheduleAfter(millis, std::forward<decltype(fn)>(fn));
    }

    [[nodiscard]] auto
    scheduleAfter(SomeStdDuration auto delay, SomeHandlerWithSignature<void(bool)> auto&& fn) noexcept
    {
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(delay);
        return pimpl_->scheduleAfter(millis, std::forward<decltype(fn)>(fn));
    }

    [[nodiscard]] auto
    makeStrand()
    {
        return pimpl_->makeStrand();
    }

private:
    struct Concept {
        virtual ~Concept() = default;

        virtual detail::ErasedOperation
        execute(
            std::function<detail::Any(AnyStopToken)>,
            std::optional<std::chrono::milliseconds> timeout = std::nullopt
        ) = 0;
        virtual detail::ErasedOperation execute(std::function<detail::Any()>) = 0;
        virtual AnyTimer scheduleAfter(std::chrono::milliseconds, std::function<void()>) = 0;
        virtual AnyTimer scheduleAfter(std::chrono::milliseconds, std::function<void(bool)>) = 0;
        virtual AnyStrand
        makeStrand() = 0;
    };

    template <typename CtxType>
    struct Model : Concept {
        std::reference_wrapper<std::decay_t<CtxType>> ctx;

        Model(CtxType& ctx) : ctx{std::ref(ctx)}
        {
        }

        detail::ErasedOperation
        execute(std::function<detail::Any(AnyStopToken)> fn, std::optional<std::chrono::milliseconds> timeout) override
        {
            return ctx.get().execute(std::move(fn), timeout);
        }

        detail::ErasedOperation
        execute(std::function<detail::Any()> fn) override
        {
            return ctx.get().execute(std::move(fn));
        }

        AnyTimer
        scheduleAfter(std::chrono::milliseconds delay, std::function<void()> fn) override
        {
            return ctx.get().scheduleAfter(delay, std::move(fn));
        }

        AnyTimer
        scheduleAfter(std::chrono::milliseconds delay, std::function<void(bool)> fn) override
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
