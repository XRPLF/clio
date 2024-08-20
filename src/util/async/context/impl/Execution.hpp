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
#include "util/async/context/impl/Timer.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

namespace util::async::impl {

struct SpawnDispatchStrategy {
    template <typename ContextType, SomeOutcome OutcomeType>
    [[nodiscard]] static auto
    dispatch(ContextType& ctx, OutcomeType&& outcome, auto&& fn)
    {
        auto op = outcome.getOperation();

        boost::asio::spawn(
            ctx.getExecutor(),
            [outcome = std::forward<decltype(outcome)>(outcome),
             fn = std::forward<decltype(fn)>(fn)](auto yield) mutable {
                if constexpr (SomeStoppableOutcome<OutcomeType>) {
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

struct PostDispatchStrategy {
    template <typename ContextType, SomeOutcome OutcomeType>
    [[nodiscard]] static auto
    dispatch(ContextType& ctx, OutcomeType&& outcome, auto&& fn)
    {
        auto op = outcome.getOperation();

        boost::asio::post(
            ctx.getExecutor(),
            [outcome = std::forward<decltype(outcome)>(outcome), fn = std::forward<decltype(fn)>(fn)]() mutable {
                if constexpr (SomeStoppableOutcome<OutcomeType>) {
                    auto& stopSource = outcome.getStopSource();
                    fn(outcome, stopSource, stopSource.getToken());
                } else {
                    fn(outcome);
                }
            }
        );

        return op;
    }
};

struct SyncDispatchStrategy {
    template <typename ContextType, SomeOutcome OutcomeType>
    [[nodiscard]] static auto
    dispatch([[maybe_unused]] ContextType& ctx, OutcomeType outcome, auto&& fn)
    {
        auto op = outcome.getOperation();

        if constexpr (SomeStoppableOutcome<OutcomeType>) {
            auto& stopSource = outcome.getStopSource();
            fn(outcome, stopSource, stopSource.getToken());
        } else {
            fn(outcome);
        }

        return op;
    }
};

}  // namespace util::async::impl
