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

#include "util/requests/Retry.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <utility>

namespace util::requests {

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
Retry::getNumRetries() const
{
    return numRetries_;
}

ExponentialBackoff::ExponentialBackoff(
    std::chrono::steady_clock::duration delay,
    std::chrono::steady_clock::duration maxDelay
)
    : delay_(delay), maxDelay_(maxDelay)
{
}

std::chrono::steady_clock::duration
ExponentialBackoff::getDelay()
{
    auto const delay = delay_;
    delay_ = std::min(delay_ * 2, maxDelay_);
    return std::min(delay, maxDelay_);
}

}  // namespace util::requests
