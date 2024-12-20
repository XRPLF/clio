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

#include "util/Assert.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <memory>
#include <utility>

namespace util {

/**
 * @brief A class to repeat some action at a regular interval
 * @note io_context must be stopped before the Repeat object is destroyed. Otherwise it is undefined behavior
 */
class Repeat {
    boost::asio::steady_timer timer_;
    std::shared_ptr<std::atomic_bool> stopping_ = std::make_shared<std::atomic_bool>(true);

public:
    /**
     * @brief Construct a new Repeat object
     * @note The `ctx` parameter is `auto` so that this util supports `strand` and `thread_pool` as well as `io_context`
     *
     * @param ctx The io_context-like object to use
     */
    Repeat(auto& ctx) : timer_(ctx)
    {
    }

    ~Repeat();

    Repeat(Repeat const&) = delete;
    Repeat&
    operator=(Repeat const&) = delete;
    Repeat(Repeat&&) = default;
    Repeat&
    operator=(Repeat&&) = default;

    /**
     * @brief Stop repeating
     * @note This method will block to ensure the repeating is actually stopped. But blocking time should be very short.
     */
    void
    stop();

    /**
     * @brief Start asynchronously repeating
     * @note stop() must be called before start() is called for the second time
     *
     * @tparam Action The action type
     * @param interval The interval to repeat
     * @param action The action to call regularly
     */
    template <std::invocable Action>
    void
    start(std::chrono::steady_clock::duration interval, Action&& action)
    {
        ASSERT(*stopping_, "Should be stopped before starting");
        *stopping_ = false;
        startImpl(interval, std::forward<Action>(action), stopping_, timer_);
    }

private:
    template <std::invocable Action>
    static void
    startImpl(
        std::chrono::steady_clock::duration interval,
        Action&& action,
        std::shared_ptr<std::atomic_bool> stopping,
        boost::asio::steady_timer& timer
    )
    {
        if (*stopping) {
            std::cout << "Exit 1" << std::endl;
            return;
        }

        timer.expires_after(interval);
        timer.async_wait(
            [interval, action = std::forward<Action>(action), &timer, stopping = std::move(stopping)](auto&&) mutable {
                if (*stopping) {
                    std::cout << "Exit 2" << std::endl;
                    return;
                }
                std::cout << "action()" << std::endl;
                action();

                std::cout << "restart" << std::endl;
                startImpl(interval, std::forward<Action>(action), std::move(stopping), timer);
            }
        );
    }
};

}  // namespace util
