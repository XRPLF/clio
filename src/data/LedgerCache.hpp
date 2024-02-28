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

#include "data/Types.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <ripple/basics/base_uint.h>
#include <ripple/basics/hardened_hash.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

namespace data {

/**
 * @brief Cache for an entire ledger.
 */
class LedgerCache {
    struct CacheEntry {
        uint32_t seq = 0;
        Blob blob;
    };

    // counters for fetchLedgerObject(s) hit rate
    std::reference_wrapper<util::prometheus::CounterInt> objectReqCounter_{PrometheusService::counterInt(
        "ledger_cache_counter_total_number",
        util::prometheus::Labels({{"type", "request"}, {"fetch", "ledger_objects"}}),
        "LedgerCache statistics"
    )};
    std::reference_wrapper<util::prometheus::CounterInt> objectHitCounter_{PrometheusService::counterInt(
        "ledger_cache_counter_total_number",
        util::prometheus::Labels({{"type", "cache_hit"}, {"fetch", "ledger_objects"}})
    )};

    // counters for fetchSuccessorKey hit rate
    std::reference_wrapper<util::prometheus::CounterInt> successorReqCounter_{PrometheusService::counterInt(
        "ledger_cache_counter_total_number",
        util::prometheus::Labels({{"type", "request"}, {"fetch", "successor_key"}}),
        "ledgerCache"
    )};
    std::reference_wrapper<util::prometheus::CounterInt> successorHitCounter_{PrometheusService::counterInt(
        "ledger_cache_counter_total_number",
        util::prometheus::Labels({{"type", "cache_hit"}, {"fetch", "successor_key"}})
    )};

    std::map<ripple::uint256, CacheEntry> map_;

    mutable std::shared_mutex mtx_;
    std::condition_variable_any cv_;
    uint32_t latestSeq_ = 0;
    std::atomic_bool full_ = false;
    std::atomic_bool disabled_ = false;

    // temporary set to prevent background thread from writing already deleted data. not used when cache is full
    std::unordered_set<ripple::uint256, ripple::hardened_hash<>> deletes_;

public:
    /**
     * @brief Update the cache with new ledger objects.
     *
     * @param objs The ledger objects to update cache with
     * @param seq The sequence to update cache for
     * @param isBackground Should be set to true when writing old data from a background thread
     */
    void
    update(std::vector<LedgerObject> const& objs, uint32_t seq, bool isBackground = false);

    /**
     * @brief Fetch a cached object by its key and sequence number.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return If found in cache, will return the cached Blob; otherwise nullopt is returned
     */
    std::optional<Blob>
    get(ripple::uint256 const& key, uint32_t seq) const;

    /**
     * @brief Gets a cached successor.
     *
     * Note: This function always returns std::nullopt when @ref isFull() returns false.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return If found in cache, will return the cached successor; otherwise nullopt is returned
     */
    std::optional<LedgerObject>
    getSuccessor(ripple::uint256 const& key, uint32_t seq) const;

    /**
     * @brief Gets a cached predcessor.
     *
     * Note: This function always returns std::nullopt when @ref isFull() returns false.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return If found in cache, will return the cached predcessor; otherwise nullopt is returned
     */
    std::optional<LedgerObject>
    getPredecessor(ripple::uint256 const& key, uint32_t seq) const;

    /**
     * @brief Disables the cache.
     */
    void
    setDisabled();

    /**
     * @brief Sets the full flag to true.
     *
     * This is used when cache loaded in its entirety at startup of the application. This can be either loaded from DB,
     * populated together with initial ledger download (on first run) or downloaded from a peer node (specified in
     * config).
     */
    void
    setFull();

    /**
     * @return The latest ledger sequence for which cache is available.
     */
    uint32_t
    latestLedgerSequence() const;

    /**
     * @return true if the cache has all data for the most recent ledger; false otherwise
     */
    bool
    isFull() const;

    /**
     * @return The total size of the cache.
     */
    size_t
    size() const;

    /**
     * @return A number representing the success rate of hitting an object in the cache versus missing it.
     */
    float
    getObjectHitRate() const;

    /**
     * @return A number representing the success rate of hitting a successor in the cache versus missing it.
     */
    float
    getSuccessorHitRate() const;

    void
    waitUntilCacheContainsSeq(uint32_t seq);
};

}  // namespace data
