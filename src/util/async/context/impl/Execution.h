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
#include <boost/asio/spawn.hpp>
#include <util/Expected.h>
#include <util/async/Concepts.h>
#include <util/async/Error.h>
#include <util/async/Outcome.h>

namespace util::async::detail {

template <typename ExecutorType>
struct CoroDispatcher {
    ExecutorType executor;

    template <SomeOutcome OutcomeType>
    auto
    dispatch(OutcomeType outcome, auto&& fn)
    {
        auto op = outcome.getOperation();

        boost::asio::spawn(
            executor,
            [outcome = std::move(outcome), fn = std::forward<decltype(fn)>(fn)](auto yield) mutable {
                if constexpr (SomeCancellableOutcome<OutcomeType>) {
                    auto& stopSource = outcome.getStopSource();
                    fn(outcome, stopSource, stopSource[yield]);
                } else {
                    fn(outcome);
                }
            }
        );

        return op;
    }
};

template <typename ExecutorType>
struct PoolDispatcher {
    ExecutorType executor;

    template <SomeOutcome OutcomeType>
    auto
    dispatch(OutcomeType outcome, auto&& fn)
    {
        auto op = outcome.getOperation();

        boost::asio::post(executor, [outcome = std::move(outcome), fn = std::forward<decltype(fn)>(fn)]() mutable {
            if constexpr (SomeCancellableOutcome<OutcomeType>) {
                auto& stopSource = outcome.getStopSource();
                fn(outcome, stopSource, stopSource.getToken());
            } else {
                fn(outcome);
            }
        });

        return op;
    }
};

struct SyncDispatcher {
    template <SomeOutcome OutcomeType>
    auto
    dispatch(OutcomeType outcome, auto&& fn)
    {
        auto op = outcome.getOperation();

        if constexpr (SomeCancellableOutcome<OutcomeType>) {
            auto& stopSource = outcome.getStopSource();
            fn(outcome, stopSource, stopSource.getToken());
        } else {
            fn(outcome);
        }

        return op;
    }
};

template <SomeStopSource StopSourceType>
[[nodiscard]] inline constexpr auto
outcomeForHandler(auto&& fn)
{
    if constexpr (SomeHandlerWithStopToken<decltype(fn)>) {
        using FnRetType = decltype(fn(std::declval<typename StopSourceType::Token>()));
        using RetType = util::Expected<FnRetType, ExecutionContextException>;

        return CancellableOutcome<RetType, StopSourceType>();
    } else {
        using FnRetType = decltype(fn());
        using RetType = util::Expected<FnRetType, ExecutionContextException>;

        return Outcome<RetType>();
    }
}

}  // namespace util::async::detail
