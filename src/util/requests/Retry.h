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
#include <boost/asio/strand.hpp>

#include <chrono>
#include <cstddef>
#include <memory>

namespace util::requests {

class RetryStrategy {
    std::chrono::steady_clock::duration delay_;

public:
    RetryStrategy(std::chrono::steady_clock::duration delay);
    virtual ~RetryStrategy() = default;

    std::chrono::steady_clock::duration
    getDelay() const;

    void
    increaseDelay();

    virtual std::chrono::steady_clock::duration
    nextDelay() const = 0;
};
using RetryStrategyPtr = std::unique_ptr<RetryStrategy>;

class Retry {
    RetryStrategyPtr strategy_;
    boost::asio::steady_timer timer_;
    size_t attemptNumber_ = 0;

public:
    Retry(RetryStrategyPtr strategy, boost::asio::strand<boost::asio::io_context::executor_type> strand);
    ~Retry();

    template <typename Fn>
    void
    retry(Fn&& func)
    {
        timer_.expires_after(strategy_->getDelay());
        strategy_->increaseDelay();
        timer_.async_wait([this, func = std::forward<Fn>(func)](boost::system::error_code const& ec) {
            if (ec) {
                return;
            }
            ++attemptNumber_;
            func();
        });
    }

    void
    cancel();

    size_t
    attemptNumber() const;

    std::chrono::steady_clock::duration
    currentDelay() const;

    std::chrono::steady_clock::duration
    nextDelay() const;
};

class ExponentialBackoff : public RetryStrategy {
    std::chrono::steady_clock::duration maxDelay_;

public:
    ExponentialBackoff(std::chrono::steady_clock::duration delay, std::chrono::steady_clock::duration maxDelay);

    std::chrono::steady_clock::duration
    nextDelay() const override;
};

Retry
makeRetryExponentialBackoff(
    std::chrono::steady_clock::duration delay,
    std::chrono::steady_clock::duration maxDelay,
    boost::asio::strand<boost::asio::io_context::executor_type> strand
);

}  // namespace util::requests
