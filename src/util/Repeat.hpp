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
#include <semaphore>

namespace util {

/**
 * @brief A class to repeat some action at a regular interval
 * @note io_context must be stopped before the Repeat object is destroyed. Otherwise it is undefined behavior
 */
class Repeat {
    struct Control {
        boost::asio::steady_timer timer;
        std::atomic_bool stopping{true};
        std::binary_semaphore semaphore{0};

        Control(auto& ctx) : timer(ctx)
        {
        }
    };

    std::unique_ptr<Control> control_;

public:
    /**
     * @brief Construct a new Repeat object
     * @note The `ctx` parameter is `auto` so that this util supports `strand` and `thread_pool` as well as `io_context`
     *
     * @param ctx The io_context-like object to use
     */
    Repeat(auto& ctx) : control_(std::make_unique<Control>(ctx))
    {
    }

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
        ASSERT(control_->stopping, "Should be stopped before starting");
        control_->stopping = false;
        startImpl(interval, std::forward<Action>(action));
    }

private:
    template <std::invocable Action>
    void
    startImpl(std::chrono::steady_clock::duration interval, Action&& action)
    {
        control_->timer.expires_after(interval);
        control_->timer.async_wait([this, interval, action = std::forward<Action>(action)](auto const& ec) mutable {
            if (ec or control_->stopping) {
                control_->semaphore.release();
                return;
            }
            action();

            startImpl(interval, std::forward<Action>(action));
        });
    }
};

}  // namespace util
