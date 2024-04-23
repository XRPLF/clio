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

#include "etl/SystemState.hpp"
#include "util/log/Logger.hpp"

#include <functional>

namespace etl {

/**
 * @brief A helper to notify Clio operator about a corruption in the DB
 *
 * @tparam CacheType The type of the cache to disable on corruption
 */
template <typename CacheType>
class CorruptionDetector {
    std::reference_wrapper<SystemState> state_;
    std::reference_wrapper<CacheType> cache_;

    util::Logger log_{"ETL"};

public:
    /**
     * @brief Construct a new Corruption Detector object
     *
     * @param state The system state
     * @param cache The cache to disable on corruption
     */
    CorruptionDetector(SystemState& state, CacheType& cache) : state_{std::ref(state)}, cache_{std::ref(cache)}
    {
    }

    /**
     * @brief Notify the operator about a corruption in the DB.
     */
    void
    onCorruptionDetected()
    {
        if (not state_.get().isCorruptionDetected) {
            state_.get().isCorruptionDetected = true;

            LOG(log_.error()) << "Disabling the cache to avoid corrupting the DB further. Please investigate.";
            cache_.get().setDisabled();
        }
    }
};

}  // namespace etl
