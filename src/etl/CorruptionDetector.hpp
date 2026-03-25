#pragma once

#include "data/LedgerCacheInterface.hpp"
#include "etl/SystemState.hpp"
#include "util/log/Logger.hpp"

#include <functional>

namespace etl {

/**
 * @brief A helper to notify Clio operator about a corruption in the DB
 *
 * @tparam CacheType The type of the cache to disable on corruption
 */
class CorruptionDetector {
    std::reference_wrapper<SystemState> state_;
    std::reference_wrapper<data::LedgerCacheInterface> cache_;

    util::Logger log_{"ETL"};

public:
    /**
     * @brief Construct a new Corruption Detector object
     *
     * @param state The system state
     * @param cache The cache to disable on corruption
     */
    CorruptionDetector(SystemState& state, data::LedgerCacheInterface& cache)
        : state_{std::ref(state)}, cache_{std::ref(cache)}
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

            LOG(
                log_.error()
            ) << "Disabling the cache to avoid corrupting the DB further. Please investigate.";
            cache_.get().setDisabled();
        }
    }
};

}  // namespace etl
