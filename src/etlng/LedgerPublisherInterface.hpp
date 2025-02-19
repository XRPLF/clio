//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include <chrono>
#include <cstdint>
#include <optional>

namespace etlng {

/**
 * @brief The interface of a scheduler for the extraction proccess
 */
struct LedgerPublisherInterface {
    virtual ~LedgerPublisherInterface() = default;

    /**
     * @brief Publish the ledger by its sequence number
     *
     * @param seq The sequence number of the ledger
     * @param maxAttempts The maximum number of attempts to publish the ledger; no limit if nullopt
     * @param attemptsDelay The delay between attempts
     */
    virtual void
    publish(
        uint32_t seq,
        std::optional<uint32_t> maxAttempts,
        std::chrono::steady_clock::duration attemptsDelay = std::chrono::seconds{1}
    ) = 0;
};

}  // namespace etlng
