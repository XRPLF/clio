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
#include <boost/asio/steady_timer.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <semaphore>

namespace util {

/**
 * @brief A wrapper around boost::asio::steady_timer that allows to destroy timer safely any time
 */
class Timer {
    boost::asio::io_context& ioc_;
    boost::asio::steady_timer timer_;
    std::atomic_bool shouldStop_{false};
    std::binary_semaphore stop_{0};

public:
    /**
     * @brief Construct a new Timer object
     *
     * @param ioc The io_context to use
     */
    Timer(boost::asio::io_context& ioc);

    /**
     * @brief Destroy the Timer object
     */
    ~Timer();

    /**
     * @brief Cancel the timer
     */
    void
    cancel();

    /**
     * @brief Asynchronously wait for the timer to expire
     *
     * @tparam Handler The handler type
     * @param handler The handler to call when the timer expires
     */
    template <std::invocable<boost::system::error_code> Handler>
    void
    async_wait(Handler&& handler)
    {
        timer_.async_wait([handler = std::forward<Handler>(handler), this](auto const& ec) {
            if (shouldStop_) {
                stop_.release();
                return;
            }
            handler(ec);
        });
    }

    /**
     * @brief Set the expiration time relative to now
     *
     * @param duration The duration to wait
     */
    void
    expires_after(std::chrono::steady_clock::duration const& duration);
};

}  // namespace util
