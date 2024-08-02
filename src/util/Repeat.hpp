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

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <semaphore>

namespace util {

/**
 * @brief A class to repeat some action at a regular interval
 */
class Repeat {
    boost::asio::io_context& ioc_;
    boost::asio::steady_timer timer_;
    std::atomic_bool stopping_{false};
    std::binary_semaphore semaphore_{0};

public:
    /**
     * @brief Construct a new Repeat object
     *
     * @param ioc The io_context to use
     */
    Repeat(boost::asio::io_context& ioc);

    /**
     * @brief Destroy the Timer object
     */
    ~Repeat();

    /**
     * @brief Stop repeating
     */
    void
    stop();

    /**
     * @brief Start asynchronously repeating
     *
     * @tparam Action The action type
     * @param action The action to call regularly
     */
    template <std::invocable<boost::system::error_code> Action>
    void
    start(std::chrono::steady_clock::duration interval, Action&& action)
    {
        timer_.expires_after(interval);
        timer_.async_wait([this, interval, action = std::forward<Action>(action)](auto const& ec) {
            if (stopping_) {
                semaphore_.release();
                return;
            }
            if (not ec)
                return;
            action();
            start(interval, std::forward<Action>(action));
        });
    }
};

}  // namespace util
