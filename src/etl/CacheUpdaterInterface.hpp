#pragma once

#include "data/Types.hpp"
#include "etl/Models.hpp"

#include <cstdint>
#include <vector>

namespace etl {

/**
 * @brief An interface for the Cache Updater
 */
struct CacheUpdaterInterface {
    virtual ~CacheUpdaterInterface() = default;

    /**
     * @brief Update the cache with ledger data
     * @param data The ledger data to update with
     */
    virtual void
    update(model::LedgerData const& data) = 0;

    /**
     * @brief Update the cache with ledger objects at a specific sequence
     * @param seq The ledger sequence number
     * @param objs The ledger objects to update with
     */
    virtual void
    update(uint32_t seq, std::vector<data::LedgerObject> const& objs) = 0;

    /**
     * @brief Update the cache with model objects at a specific sequence
     * @param seq The ledger sequence number
     * @param objs The model objects to update with
     */
    virtual void
    update(uint32_t seq, std::vector<model::Object> const& objs) = 0;

    /**
     * @brief Mark the cache as fully loaded
     */
    virtual void
    setFull() = 0;
};

}  // namespace etl
