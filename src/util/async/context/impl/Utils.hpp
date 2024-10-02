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

#include "util/async/Concepts.hpp"
#include "util/async/Error.hpp"
#include "util/async/context/impl/Timer.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

#include <expected>
#include <optional>

namespace util::async::impl {

inline constexpr struct AssociatedExecutorExtractor {
    template <typename CtxType>
    [[nodiscard]] typename CtxType::ExecutorType&
    operator()(CtxType& ctx) const noexcept
    {
        return ctx.context_.getExecutor();
    }
} extractAssociatedExecutor;

template <typename CtxType>
[[nodiscard]] constexpr auto
getTimeoutHandleIfNeeded(CtxType& ctx, SomeOptStdDuration auto timeout, SomeStopSource auto& stopSource)
{
    using TimerType = typename CtxType::Timer;
    std::optional<TimerType> timer;
    if (timeout) {
        timer.emplace(extractAssociatedExecutor(ctx), *timeout, [&stopSource](auto cancelled) {
            if (not cancelled)
                stopSource.requestStop();
        });
    }
    return timer;
}

template <SomeStopSource StopSourceType>
[[nodiscard]] constexpr auto
outcomeForHandler(auto&& fn)
{
    if constexpr (SomeHandlerWith<decltype(fn), typename StopSourceType::Token>) {
        using FnRetType = decltype(fn(std::declval<typename StopSourceType::Token>()));
        using RetType = std::expected<FnRetType, ExecutionError>;

        return StoppableOutcome<RetType, StopSourceType>();
    } else {
        using FnRetType = decltype(fn());
        using RetType = std::expected<FnRetType, ExecutionError>;

        return Outcome<RetType>();
    }
}

struct SelfContextProvider {
    template <typename CtxType>
    [[nodiscard]] static constexpr auto&
    getContext(CtxType& self) noexcept
    {
        return self;
    }
};

}  // namespace util::async::impl
