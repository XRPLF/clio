//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <backend/cassandra/Handle.h>
#include <backend/cassandra/Types.h>
#include <log/Logger.h>
#include <util/Expected.h>

#include <boost/asio.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace Backend::Cassandra::detail {

/**
 * @brief A retry policy that employs exponential backoff
 */
class ExponentialBackoffRetryPolicy
{
    clio::Logger log_{"Backend"};

    boost::asio::steady_timer timer_;
    uint32_t attempt_ = 0u;

public:
    /**
     * @brief Create a new retry policy instance with the io_context provided
     */
    ExponentialBackoffRetryPolicy(boost::asio::io_context& ioc) : timer_{ioc}
    {
    }

    /**
     * @brief Computes next retry delay and returns true unconditionally
     *
     * @param err The cassandra error that triggered the retry
     */
    [[nodiscard]] bool
    shouldRetry([[maybe_unused]] CassandraError err)
    {
        auto const delay = calculateDelay(attempt_);
        log_.error() << "Cassandra write error: " << err << ", current retries "
                     << attempt_ << ", retrying in " << delay.count()
                     << " milliseconds";

        return true;  // keep retrying forever
    }

    /**
     * @brief Schedules next retry
     *
     * @param fn The callable to execute
     */
    template <typename Fn>
    void
    retry(Fn&& fn)
    {
        timer_.expires_after(calculateDelay(attempt_++));
        timer_.async_wait(
            [fn = std::move(fn)]([[maybe_unused]] const auto& err) {
                // todo: deal with cancellation (thru err)
                fn();
            });
    }

    /**
     * @brief Calculates the wait time before attempting another retry
     */
    std::chrono::milliseconds
    calculateDelay(uint32_t attempt)
    {
        return std::chrono::milliseconds{
            lround(std::pow(2, std::min(10u, attempt)))};
    }
};

}  // namespace Backend::Cassandra::detail
