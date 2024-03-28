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

#include "data/cassandra/Error.hpp"
#include "data/cassandra/Types.hpp"
#include "util/Retry.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <chrono>

namespace data::cassandra::impl {

/**
 * @brief A retry policy that employs exponential backoff
 */
class ExponentialBackoffRetryPolicy {
    util::Logger log_{"Backend"};

    util::Retry retry_;

public:
    /**
     * @brief Create a new retry policy instance with the io_context provided
     */
    ExponentialBackoffRetryPolicy(boost::asio::io_context& ioc)
        : retry_(util::makeRetryExponentialBackoff(
              std::chrono::milliseconds(1),
              std::chrono::seconds(1),
              boost::asio::make_strand(ioc)
          ))
    {
    }

    /**
     * @brief Computes next retry delay and returns true unconditionally
     *
     * @param err The cassandra error that triggered the retry
     */
    [[nodiscard]] static bool
    shouldRetry([[maybe_unused]] CassandraError err)
    {
        auto const delayMs = std::chrono::duration_cast<std::chrono::milliseconds>(retry_.delayValue()).count();
        LOG(log_.error()) << "Cassandra write error: " << err << ", current retries " << retry_.attemptNumber()
                          << ", retrying in " << delayMs << " milliseconds";

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
        retry_.retry(std::forward<Fn>(fn));
    }
};

}  // namespace data::cassandra::impl
