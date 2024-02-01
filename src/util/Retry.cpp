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

#include "util/Retry.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <utility>

namespace util {

RetryStrategy::RetryStrategy(std::chrono::steady_clock::duration delay) : delay_(delay)
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

Retry::Retry(RetryStrategyPtr strategy, boost::asio::strand<boost::asio::io_context::executor_type> strand)
    : strategy_(std::move(strategy)), timer_(strand.get_inner_executor())
{
}

Retry::~Retry()
{
    cancel();
}

void
Retry::cancel()
{
    timer_.cancel();
}

size_t
Retry::attemptNumber() const
{
    return attemptNumber_;
}

std::chrono::steady_clock::duration
Retry::currentDelay() const
{
    return strategy_->getDelay();
}

std::chrono::steady_clock::duration
Retry::nextDelay() const
{
    return strategy_->nextDelay();
}

ExponentialBackoff::ExponentialBackoff(
    std::chrono::steady_clock::duration delay,
    std::chrono::steady_clock::duration maxDelay
)
    : RetryStrategy(delay), maxDelay_(maxDelay)
{
}

std::chrono::steady_clock::duration
ExponentialBackoff::nextDelay() const
{
    auto const next = getDelay() * 2;
    return std::min(next, maxDelay_);
}

Retry
makeRetryExponentialBackoff(
    std::chrono::steady_clock::duration delay,
    std::chrono::steady_clock::duration maxDelay,
    boost::asio::strand<boost::asio::io_context::executor_type> strand
)
{
    return Retry(std::make_unique<ExponentialBackoff>(delay, maxDelay), std::move(strand));
}

}  // namespace util
