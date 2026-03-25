#pragma once

#include <cstdint>

namespace etl {

/**
 * @brief An interface for the Cache Loader
 */
struct CacheLoaderInterface {
    virtual ~CacheLoaderInterface() = default;

    /**
     * @brief Load the cache with the most recent ledger data
     *
     * @param seq The sequence number of the ledger to load
     */
    virtual void
    load(uint32_t const seq) = 0;

    /**
     * @brief Stop the cache loading process
     */
    virtual void
    stop() noexcept = 0;

    /**
     * @brief Wait for all cache loading tasks to complete
     */
    virtual void
    wait() noexcept = 0;
};

}  // namespace etl
