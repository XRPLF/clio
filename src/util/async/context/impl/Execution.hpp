#pragma once

#include "util/Spawn.hpp"
#include "util/async/Concepts.hpp"
#include "util/async/context/impl/Timer.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/thread_pool.hpp>

namespace util::async::impl {

struct SpawnDispatchStrategy {
    template <typename ContextType, SomeOutcome OutcomeType, typename FnType>
    [[nodiscard]] static auto
    dispatch(ContextType& ctx, OutcomeType&& outcome, FnType&& fn)
    {
        auto op = outcome.getOperation();

        if constexpr (SomeStoppableOutcome<OutcomeType>) {
            util::spawn(
                ctx.getExecutor(),
                [outcome = std::forward<OutcomeType>(outcome),
                 fn = std::forward<FnType>(fn)](auto yield) mutable {
                    if constexpr (SomeStoppableOutcome<OutcomeType>) {
                        auto& stopSource = outcome.getStopSource();
                        std::invoke(
                            std::forward<decltype(fn)>(fn), outcome, stopSource, stopSource[yield]
                        );
                    } else {
                        std::invoke(std::forward<decltype(fn)>(fn), outcome);
                    }
                }
            );
        } else {
            boost::asio::post(
                ctx.getExecutor(),
                [outcome = std::forward<OutcomeType>(outcome),
                 fn = std::forward<FnType>(fn)]() mutable {
                    std::invoke(std::forward<decltype(fn)>(fn), outcome);
                }
            );
        }

        return op;
    }

    template <typename ContextType, typename FnType>
    static void
    post(ContextType& ctx, FnType&& fn)
    {
        boost::asio::post(ctx.getExecutor(), [fn = std::forward<FnType>(fn)]() mutable {
            std::invoke(std::forward<decltype(fn)>(fn));
        });
    }
};

struct PostDispatchStrategy {
    template <typename ContextType, SomeOutcome OutcomeType, typename FnType>
    [[nodiscard]] static auto
    dispatch(ContextType& ctx, OutcomeType&& outcome, FnType&& fn)
    {
        auto op = outcome.getOperation();

        boost::asio::post(
            ctx.getExecutor(),
            [outcome = std::forward<OutcomeType>(outcome),
             fn = std::forward<FnType>(fn)]() mutable {
                if constexpr (SomeStoppableOutcome<OutcomeType>) {
                    auto& stopSource = outcome.getStopSource();
                    std::invoke(
                        std::forward<decltype(fn)>(fn), outcome, stopSource, stopSource.getToken()
                    );
                } else {
                    std::invoke(std::forward<decltype(fn)>(fn), outcome);
                }
            }
        );

        return op;
    }

    template <typename ContextType, typename FnType>
    static void
    post(ContextType& ctx, FnType&& fn)
    {
        boost::asio::post(ctx.getExecutor(), std::forward<FnType>(fn));
    }
};

struct SyncDispatchStrategy {
    template <typename ContextType, SomeOutcome OutcomeType, typename FnType>
    [[nodiscard]] static auto
    dispatch([[maybe_unused]] ContextType& ctx, OutcomeType outcome, FnType&& fn)
    {
        auto op = outcome.getOperation();

        if constexpr (SomeStoppableOutcome<OutcomeType>) {
            auto& stopSource = outcome.getStopSource();
            std::invoke(std::forward<FnType>(fn), outcome, stopSource, stopSource.getToken());
        } else {
            std::invoke(std::forward<FnType>(fn), outcome);
        }

        return op;
    }

    template <typename ContextType, typename FnType>
    static void
    post([[maybe_unused]] ContextType& ctx, FnType&& fn)
    {
        std::invoke(std::forward<FnType>(fn));
    }
};

}  // namespace util::async::impl
