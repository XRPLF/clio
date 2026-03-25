#pragma once

#include "util/async/Concepts.hpp"
#include "util/async/Operation.hpp"
#include "util/async/context/impl/Cancellation.hpp"
#include "util/async/context/impl/Execution.hpp"
#include "util/async/context/impl/Timer.hpp"
#include "util/async/context/impl/Utils.hpp"
#include "util/async/impl/ErrorHandling.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <type_traits>

namespace util::async::impl {

template <
    typename ParentContextType,
    typename StopSourceType,
    typename DispatcherType,
    typename TimerContextProvider = impl::SelfContextProvider,
    typename ErrorHandlerType = impl::DefaultErrorHandler>
class BasicStrand {
    std::reference_wrapper<ParentContextType> parentContext_;
    typename ParentContextType::ContextHolderType::Strand context_;
    friend AssociatedExecutorExtractor;

public:
    static constexpr bool kIS_NOEXCEPT = noexcept(ErrorHandlerType::wrap([](auto&) { throw 0; }));

    using ContextHolderType = typename ParentContextType::ContextHolderType::Strand;
    using ExecutorType = typename ContextHolderType::Executor;
    using StopToken = typename StopSourceType::Token;
    using Timer = typename ParentContextType::ContextHolderType::Timer;  // timers are associated
                                                                         // with the parent context
    using RepeatedOperation = RepeatingOperation<BasicStrand>;

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
    ) noexcept(kIS_NOEXCEPT)
    {
        return DispatcherType::dispatch(
            context_,
            impl::outcomeForHandler<StopSourceType>(fn),
            ErrorHandlerType::wrap([this, timeout, fn = std::forward<decltype(fn)>(fn)](
                                       auto& outcome, auto& stopSource, auto stopToken
                                   ) mutable {
                [[maybe_unused]] auto timeoutHandler = impl::getTimeoutHandleIfNeeded(
                    TimerContextProvider::getContext(parentContext_.get()), timeout, stopSource
                );

                using FnRetType = std::decay_t<std::invoke_result_t<decltype(fn), StopToken>>;
                if constexpr (std::is_void_v<FnRetType>) {
                    std::invoke(std::forward<decltype(fn)>(fn), std::move(stopToken));
                    outcome.setValue();
                } else {
                    outcome.setValue(
                        std::invoke(std::forward<decltype(fn)>(fn), std::move(stopToken))
                    );
                }
            })
        );
    }

    [[nodiscard]] auto
    execute(SomeHandlerWith<StopToken> auto&& fn, SomeStdDuration auto timeout) noexcept(
        kIS_NOEXCEPT
    )
    {
        return execute(
            std::forward<decltype(fn)>(fn),
            std::make_optional(std::chrono::duration_cast<std::chrono::milliseconds>(timeout))
        );
    }

    [[nodiscard]] auto
    execute(SomeHandlerWithoutStopToken auto&& fn) noexcept(kIS_NOEXCEPT)
    {
        return DispatcherType::dispatch(
            context_,
            impl::outcomeForHandler<StopSourceType>(fn),
            ErrorHandlerType::wrap([fn = std::forward<decltype(fn)>(fn)](auto& outcome) mutable {
                using FnRetType = std::decay_t<std::invoke_result_t<decltype(fn)>>;
                if constexpr (std::is_void_v<FnRetType>) {
                    std::invoke(std::forward<decltype(fn)>(fn));
                    outcome.setValue();
                } else {
                    outcome.setValue(std::invoke(std::forward<decltype(fn)>(fn)));
                }
            })
        );
    }

    [[nodiscard]] auto
    executeRepeatedly(
        SomeStdDuration auto interval,
        SomeHandlerWithoutStopToken auto&& fn
    ) noexcept(kIS_NOEXCEPT)
    {
        if constexpr (not std::is_same_v<
                          decltype(TimerContextProvider::getContext(*this)),
                          decltype(*this)>) {
            return TimerContextProvider::getContext(*this).executeRepeatedly(
                interval, std::forward<decltype(fn)>(fn)
            );
        } else {
            return RepeatedOperation(
                impl::extractAssociatedExecutor(*this), interval, std::forward<decltype(fn)>(fn)
            );
        }
    }

    void
    submit(SomeHandlerWithoutStopToken auto&& fn) noexcept(kIS_NOEXCEPT)
    {
        DispatcherType::post(context_, ErrorHandlerType::catchAndAssert(fn));
    }
};

}  // namespace util::async::impl
