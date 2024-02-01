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

namespace util {

/**
 * @brief Interface for retry strategies
 */
class RetryStrategy {
    std::chrono::steady_clock::duration delay_;

public:
    RetryStrategy(std::chrono::steady_clock::duration delay);
    virtual ~RetryStrategy() = default;

    /**
     * @brief Get the current delay value
     *
     * @return std::chrono::steady_clock::duration
     */
    std::chrono::steady_clock::duration
    getDelay() const;

    /**
     * @brief Increase the delay value
     */
    void
    increaseDelay();

protected:
    /**
     * @brief Compute the next delay value
     *
     * @return std::chrono::steady_clock::duration
     */
    virtual std::chrono::steady_clock::duration
    nextDelay() const = 0;
};
using RetryStrategyPtr = std::unique_ptr<RetryStrategy>;

/**
 * @brief A retry mechanism
 */
class Retry {
    RetryStrategyPtr strategy_;
    boost::asio::steady_timer timer_;
    size_t attemptNumber_ = 0;

public:
    Retry(RetryStrategyPtr strategy, boost::asio::strand<boost::asio::io_context::executor_type> strand);
    ~Retry();

    /**
     * @brief Schedule a retry
     *
     * @tparam Fn The type of the callable to execute
     * @param func The callable to execute
     */
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

    /**
     * @brief Cancel scheduled retry if any
     */
    void
    cancel();

    /**
     * @brief Get the current attempt number
     *
     * @return size_t
     */
    size_t
    attemptNumber() const;

    /**
     * @brief Get the current delay value
     *
     * @return std::chrono::steady_clock::duration
     */
    std::chrono::steady_clock::duration
    delayValue() const;
};

/**
 * @brief Create a retry mechanism with exponential backoff strategy
 *
 * @param delay The initial delay value
 * @param maxDelay The maximum delay value
 * @param strand The strand to use for async operations
 * @return Retry
 */
class ExponentialBackoffStrategy : public RetryStrategy {
    std::chrono::steady_clock::duration maxDelay_;

public:
    ExponentialBackoffStrategy(std::chrono::steady_clock::duration delay, std::chrono::steady_clock::duration maxDelay);

private:
    std::chrono::steady_clock::duration
    nextDelay() const override;
};

/**
 * @brief Create a retry mechanism with exponential backoff strategy
 *
 * @param delay The initial delay value
 * @param maxDelay The maximum delay value
 * @param strand The strand to use for async operations
 * @return Retry
 */
Retry
makeRetryExponentialBackoff(
    std::chrono::steady_clock::duration delay,
    std::chrono::steady_clock::duration maxDelay,
    boost::asio::strand<boost::asio::io_context::executor_type> strand
);

}  // namespace util
