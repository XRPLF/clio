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

#include <boost/asio.hpp>
#include <util/async/context/impl/Cancellation.h>
#include <util/async/context/impl/Execution.h>
#include <util/async/impl/ErrorHandling.h>

#include <memory>

namespace util::async::detail {

template <typename CtxType, template <typename> typename DispatchStrategy, typename ErrorHandlingStrategy>
class BasicStrand {
    using ExecutorType = boost::asio::strand<boost::asio::thread_pool::executor_type>;
    using StopSourceType = typename CtxType::StopSource;
    using StopToken = typename StopSourceType::Token;

    std::reference_wrapper<CtxType> ctx_;
    DispatchStrategy<ExecutorType> dispatcher_;

public:
    BasicStrand(CtxType& ctx) : ctx_{std::ref(ctx)}, dispatcher_{boost::asio::make_strand(ctx.ioc_)}
    {
    }

    ~BasicStrand() = default;
    BasicStrand(BasicStrand&&) = default;
    BasicStrand(BasicStrand const&) = delete;

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
                auto timeoutHandler = detail::getTimoutHandleIfNeeded(ctx_.get(), timeout, stopSource);

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
};

}  // namespace util::async::detail
