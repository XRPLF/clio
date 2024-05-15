//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

/** @file */
#pragma once

#include <cstdint>
#include <optional>
namespace etl {

/**
 * @brief An interface for NetworkValidatedLedgers
 */
class NetworkValidatedLedgersInterface {
public:
    virtual ~NetworkValidatedLedgersInterface() = default;

    /**
     * @brief Notify the datastructure that idx has been validated by the network.
     *
     * @param idx Sequence validated by network
     */
    virtual void
    push(uint32_t idx) = 0;

    /**
     * @brief Get most recently validated sequence.
     *
     * If no ledgers are known to have been validated, this function waits until the next ledger is validated
     *
     * @return Sequence of most recently validated ledger. empty optional if the datastructure has been stopped
     */
    virtual std::optional<uint32_t>
    getMostRecent() = 0;

    /**
     * @brief Waits for the sequence to be validated by the network.
     *
     * @param sequence The sequence to wait for
     * @param maxWaitMs Maximum time to wait for the sequence to be validated. If empty, wait indefinitely
     * @return true if sequence was validated, false otherwise a return value of false means the datastructure has been
     * stopped
     */
    virtual bool
    waitUntilValidatedByNetwork(uint32_t sequence, std::optional<uint32_t> maxWaitMs = {}) = 0;
};

}  // namespace etl
