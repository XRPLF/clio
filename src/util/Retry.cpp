#include "util/Retry.hpp"

#include "util/Assert.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <utility>

namespace util {

RetryStrategy::RetryStrategy(std::chrono::steady_clock::duration delay)
    : initialDelay_(delay), delay_(delay)
{
}

std::chrono::steady_clock::duration
RetryStrategy::getDelay() const
{
    return delay_;
}

void
RetryStrategy::increaseDelay()
{
    delay_ = nextDelay();
}

void
RetryStrategy::reset()
{
    delay_ = initialDelay_;
}

Retry::Retry(
    RetryStrategyPtr strategy,
    boost::asio::strand<boost::asio::io_context::executor_type> strand
)
    : strategy_(std::move(strategy)), timer_(strand.get_inner_executor())
{
}

Retry::Retry(RetryStrategyPtr strategy, boost::asio::any_io_executor executor)
    : strategy_(std::move(strategy)), timer_(executor)
{
}

Retry::~Retry()
{
    *canceled_ = true;
}

void
Retry::cancel()
{
    timer_.cancel();
    *canceled_ = true;
}

size_t
Retry::attemptNumber() const
{
    return attemptNumber_;
}

std::chrono::steady_clock::duration
Retry::delayValue() const
{
    return strategy_->getDelay();
}

void
Retry::reset()
{
    attemptNumber_ = 0;
    (*strategy_).reset();
}

ExponentialBackoffStrategy::ExponentialBackoffStrategy(Retry::Delays delays)
    : RetryStrategy(delays.initial), maxDelay_(delays.max)
{
}

std::chrono::steady_clock::duration
ExponentialBackoffStrategy::nextDelay() const
{
    auto const next = getDelay() * 2;
    return std::min(next, maxDelay_);
}

void
Retry::wait(boost::asio::yield_context yield)
{
    // Suspending a coroutine while an exception is being handled corrupts the caught-exception
    // state, which is per-thread rather than per-coroutine. Copy whatever is needed out of the
    // handler and let it exit before waiting.
    ASSERT(
        std::current_exception() == nullptr,
        "Retry::wait must not be called while an exception is being handled"
    );

    *canceled_ = false;
    timer_.expires_after(strategy_->getDelay());
    strategy_->increaseDelay();
    ++attemptNumber_;

    // error ignored on purpose: a cancelled timer just means the caller retries sooner
    boost::system::error_code ec;
    timer_.async_wait(yield[ec]);
}

Retry
makeRetryExponentialBackoff(
    Retry::Delays delays,
    boost::asio::strand<boost::asio::io_context::executor_type> strand
)
{
    return Retry(std::make_unique<ExponentialBackoffStrategy>(delays), std::move(strand));
}

Retry
makeRetryExponentialBackoff(Retry::Delays delays, boost::asio::any_io_executor executor)
{
    return Retry(std::make_unique<ExponentialBackoffStrategy>(delays), std::move(executor));
}

}  // namespace util
