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

#pragma once

#include "etl/NetworkValidatedLedgersInterface.hpp"

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace etl {

/**
 * @brief This datastructure is used to keep track of the sequence of the most recent ledger validated by the network.
 *
 * There are two methods that will wait until certain conditions are met. This datastructure is able to be "stopped".
 * When the datastructure is stopped, any threads currently waiting are unblocked.
 * Any later calls to methods of this datastructure will not wait. Once the datastructure is stopped, the datastructure
 * remains stopped for the rest of its lifetime.
 */
class NetworkValidatedLedgers : public NetworkValidatedLedgersInterface {
    // max sequence validated by network
    std::optional<uint32_t> max_;

    mutable std::mutex m_;
    std::condition_variable cv_;

public:
    /**
     * @brief A factory function for NetworkValidatedLedgers
     *
     * @return A shared pointer to a new instance of NetworkValidatedLedgers
     */
    static std::shared_ptr<NetworkValidatedLedgers>
    make_ValidatedLedgers();

    /**
     * @brief Notify the datastructure that idx has been validated by the network.
     *
     * @param idx Sequence validated by network
     */
    void
    push(uint32_t idx) final;

    /**
     * @brief Get most recently validated sequence.
     *
     * If no ledgers are known to have been validated, this function waits until the next ledger is validated
     *
     * @return Sequence of most recently validated ledger. empty optional if the datastructure has been stopped
     */
    std::optional<uint32_t>
    getMostRecent() final;

    /**
     * @brief Waits for the sequence to be validated by the network.
     *
     * @param sequence The sequence to wait for
     * @param maxWaitMs Maximum time to wait for the sequence to be validated. If empty, wait indefinitely
     * @return true if sequence was validated, false otherwise a return value of false means the datastructure has been
     * stopped
     */
    bool
    waitUntilValidatedByNetwork(uint32_t sequence, std::optional<uint32_t> maxWaitMs = {}) final;
};

}  // namespace etl
