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
#include "util/async/context/impl/Cancellation.h"
#include "util/async/context/impl/Execution.h"
#include "util/async/context/impl/Timer.h"
#include "util/async/context/impl/Utils.h"
#include "util/async/impl/ErrorHandling.h"

#include <chrono>
#include <functional>
#include <optional>
#include <type_traits>

namespace util::async::detail {

template <
    typename ParentContextType,  // SomeExecutionContext
    SomeStopSource StopSourceType,
    typename DispatcherType,                                      // SomeDispatchStrategy
    typename TimerContextProvider = detail::SelfContextProvider,  // SomeTimerContextProvider
    typename ErrorHandlerType = detail::DefaultErrorHandler>      // SomeErrorStrategy>
class BasicStrand {
    std::reference_wrapper<ParentContextType> parentContext_;
    ParentContextType::ContextHolderType::Strand context_;
    friend AssociatedExecutorExtractor;

public:
    using ContextHolderType = ParentContextType::ContextHolderType::Strand;
    using ExecutorType = ContextHolderType::Executor;
    using StopToken = StopSourceType::Token;
    using Timer = ParentContextType::ContextHolderType::Timer;  // timers are associated with the parent context

    BasicStrand(ParentContextType& parent, auto&& strand)
        : parentContext_{std::ref(parent)}, context_{std::forward<decltype(strand)>(strand)}
    {
    }

    ~BasicStrand() = default;
    BasicStrand(BasicStrand&&) = default;
    BasicStrand(BasicStrand const&) = delete;

    [[nodiscard]] auto
    execute(
        SomeHandlerWith<StopToken> auto&& fn,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt
    ) noexcept
    {
        return DispatcherType::dispatch(
            context_,
            detail::outcomeForHandler<StopSourceType>(fn),
            ErrorHandlerType::wrap([this, timeout, fn = std::forward<decltype(fn)>(fn)](
                                       auto& outcome, auto& stopSource, auto stopToken
                                   ) mutable {
                [[maybe_unused]] auto timeoutHandler = detail::getTimeoutHandleIfNeeded(
                    TimerContextProvider::getContext(parentContext_.get()), timeout, stopSource
                );

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
    execute(SomeHandlerWith<StopToken> auto&& fn, SomeStdDuration auto timeout) noexcept
    {
        return execute(
            std::forward<decltype(fn)>(fn),
            std::make_optional(std::chrono::duration_cast<std::chrono::milliseconds>(timeout))
        );
    }

    [[nodiscard]] auto
    execute(SomeHandlerWithoutStopToken auto&& fn) noexcept
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
};

}  // namespace util::async::detail
