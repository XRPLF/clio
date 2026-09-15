#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>

namespace util {

/**
 * @brief Interface for retry strategies
 */
class RetryStrategy {
    std::chrono::steady_clock::duration initialDelay_;
    std::chrono::steady_clock::duration delay_;

public:
    /**
     * @brief Construct a new Retry Strategy object
     *
     * @param delay The initial delay value
     */
    RetryStrategy(std::chrono::steady_clock::duration delay);
    virtual ~RetryStrategy() = default;

    /**
     * @return The current delay value
     */
    [[nodiscard]] std::chrono::steady_clock::duration
    getDelay() const;

    /**
     * @brief Increase the delay value
     */
    void
    increaseDelay();

    /**
     * @brief Reset the delay value
     */
    void
    reset();

protected:
    /**
     * @return The next computed delay value
     */
    [[nodiscard]] virtual std::chrono::steady_clock::duration
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
    std::shared_ptr<std::atomic_bool> canceled_{std::make_shared<std::atomic_bool>(false)};

public:
    /**
     * @brief The delays to use between retry attempts
     *
     * Equal `initial` and `max` mean a flat delay with no backoff.
     */
    struct Delays {
        std::chrono::steady_clock::duration initial;
        std::chrono::steady_clock::duration max;
    };

    /**
     * @brief Construct a new Retry object
     *
     * @param strategy The retry strategy to use
     * @param strand The strand to use for async operations
     */
    Retry(
        RetryStrategyPtr strategy,
        boost::asio::strand<boost::asio::io_context::executor_type> strand
    );

    /**
     * @brief Construct a new Retry object from any I/O executor
     *
     * For coroutines, pass `yield.get_executor()` and drive it with @ref wait().
     *
     * @param strategy The retry strategy to use
     * @param executor The executor to run the retry timer on
     */
    Retry(RetryStrategyPtr strategy, boost::asio::any_io_executor executor);

    /**
     * @brief Destroy the Retry object
     */
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
        *canceled_ = false;
        timer_.expires_after(strategy_->getDelay());
        strategy_->increaseDelay();
        timer_.async_wait([this,
                           canceled = canceled_,
                           func = std::forward<Fn>(func)](boost::system::error_code const& ec) {
            if (ec == boost::asio::error::operation_aborted or *canceled) {
                return;
            }
            ++attemptNumber_;
            func();
        });
    }

    /**
     * @brief Wait out the current delay by suspending the calling coroutine, then back off.
     *
     * Unlike @ref retry() this returns once the delay elapsed instead of scheduling a callback, so
     * the caller can keep its own loop. Advances the delay and attempt number like @ref retry().
     *
     * @param yield The coroutine to suspend
     */
    void
    wait(boost::asio::yield_context yield);

    /**
     * @brief Cancel scheduled retry if any
     */
    void
    cancel();

    /**
     * @return The current attempt number
     */
    [[nodiscard]] size_t
    attemptNumber() const;

    /**
     * @return The current delay value
     */
    [[nodiscard]] std::chrono::steady_clock::duration
    delayValue() const;

    /**
     * @brief Reset the delay value and attempt number
     */
    void
    reset();
};

/**
 * @brief A retry strategy that retries while exponentially increasing the delay between attempts
 */
class ExponentialBackoffStrategy : public RetryStrategy {
    std::chrono::steady_clock::duration maxDelay_;

public:
    /**
     * @brief Construct a new Exponential Backoff Strategy object
     *
     * @param delays The delays to use between attempts
     */
    explicit ExponentialBackoffStrategy(Retry::Delays delays);

private:
    [[nodiscard]] std::chrono::steady_clock::duration
    nextDelay() const override;
};

/**
 * @brief Create a retry mechanism with exponential backoff strategy
 *
 * @param delays The delays to use between attempts
 * @param strand The strand to use for async operations
 * @return The retry object
 */
Retry
makeRetryExponentialBackoff(
    Retry::Delays delays,
    boost::asio::strand<boost::asio::io_context::executor_type> strand
);

/**
 * @brief Create a retry mechanism with exponential backoff strategy on any I/O executor
 *
 * @param delays The delays to use between attempts
 * @param executor The executor to run the retry timer on
 * @return The retry object
 */
Retry
makeRetryExponentialBackoff(Retry::Delays delays, boost::asio::any_io_executor executor);

}  // namespace util
