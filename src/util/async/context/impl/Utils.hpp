#pragma once

#include "util/async/Concepts.hpp"
#include "util/async/Error.hpp"
#include "util/async/context/impl/Timer.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

#include <expected>
#include <optional>
#include <type_traits>

namespace util::async::impl {

inline constexpr struct AssociatedExecutorExtractor {
    template <typename CtxType>
    [[nodiscard]] typename CtxType::ExecutorType&
    operator()(CtxType& ctx) const noexcept
    {
        return ctx.context_.getExecutor();
    }
} extractAssociatedExecutor;  // NOLINT(readability-identifier-naming)

template <typename CtxType>
[[nodiscard]] constexpr auto
getTimeoutHandleIfNeeded(
    CtxType& ctx,
    SomeOptStdDuration auto timeout,
    SomeStopSource auto& stopSource
)
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
        using FnRetType =
            std::decay_t<std::invoke_result_t<decltype(fn), typename StopSourceType::Token>>;
        using RetType = std::expected<FnRetType, ExecutionError>;

        return StoppableOutcome<RetType, StopSourceType>();
    } else {
        using FnRetType = std::decay_t<std::invoke_result_t<decltype(fn)>>;
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
