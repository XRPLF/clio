#pragma once

#include "data/LedgerCacheInterface.hpp"
#include "data/Types.hpp"
#include "etl/Models.hpp"
#include "util/prometheus/Bool.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/hardened_hash.h>

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace data {

/**
 * @brief Cache for an entire ledger.
 */
class LedgerCache : public LedgerCacheInterface {
public:
    /**
     * @brief An entry of the cache
     */
    struct CacheEntry {
        uint32_t seq = 0;  ///< Ledger sequence at which the object was last updated
        Blob blob;         ///< Serialized ledger object data
    };

    /** @brief Map from ledger object key to its cache entry */
    using CacheMap = std::map<xrpl::uint256, CacheEntry>;

private:
    // counters for fetchLedgerObject(s) hit rate
    std::reference_wrapper<util::prometheus::CounterInt> objectReqCounter_{
        PrometheusService::counterInt(
            "ledger_cache_counter_total_number",
            util::prometheus::Labels({{"type", "request"}, {"fetch", "ledger_objects"}}),
            "LedgerCache statistics"
        )
    };
    std::reference_wrapper<util::prometheus::CounterInt> objectHitCounter_{
        PrometheusService::counterInt(
            "ledger_cache_counter_total_number",
            util::prometheus::Labels({{"type", "cache_hit"}, {"fetch", "ledger_objects"}})
        )
    };

    // counters for fetchSuccessorKey hit rate
    std::reference_wrapper<util::prometheus::CounterInt> successorReqCounter_{
        PrometheusService::counterInt(
            "ledger_cache_counter_total_number",
            util::prometheus::Labels({{"type", "request"}, {"fetch", "successor_key"}}),
            "ledgerCache"
        )
    };
    std::reference_wrapper<util::prometheus::CounterInt> successorHitCounter_{
        PrometheusService::counterInt(
            "ledger_cache_counter_total_number",
            util::prometheus::Labels({{"type", "cache_hit"}, {"fetch", "successor_key"}})
        )
    };

    CacheMap map_;
    CacheMap deleted_;

    mutable std::shared_mutex mtx_;
    std::condition_variable_any cv_;
    uint32_t latestSeq_ = 0;
    util::prometheus::Bool full_{
        PrometheusService::boolMetric(
            "ledger_cache_full",
            util::prometheus::Labels{},
            "Whether ledger cache full or not"
        ),
    };
    util::prometheus::Bool disabled_{
        PrometheusService::boolMetric(
            "ledger_cache_disabled",
            util::prometheus::Labels{},
            "Whether ledger cache is disabled or not"
        ),
    };
    util::prometheus::Bool isCurrentlyLoading_{
        PrometheusService::boolMetric(
            "ledger_cache_is_currently_loading",
            util::prometheus::Labels{},
            "Whether ledger cache is currently loading or not"
        ),

    };

    // temporary set to prevent background thread from writing already deleted data. not used when
    // cache is full
    std::unordered_set<xrpl::uint256, xrpl::HardenedHash<>> deletes_;

public:
    /**
     * @brief Update the cache with new ledger objects.
     *
     * @param objs The ledger objects to update cache with
     * @param seq The sequence to update cache for
     * @param isBackground Should be set to true when writing old data from a background thread
     */
    void
    update(std::vector<LedgerObject> const& objs, uint32_t seq, bool isBackground) override;

    /**
     * @brief Update the cache with new ledger objects.
     *
     * @param objs The ledger objects to update cache with
     * @param seq The sequence to update cache for
     */
    void
    update(std::vector<etl::model::Object> const& objs, uint32_t seq) override;

    /**
     * @brief Fetch a cached object by its key and sequence number.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return The cached Blob if found; nullopt otherwise
     */
    std::optional<Blob>
    get(xrpl::uint256 const& key, uint32_t seq) const override;

    /**
     * @brief Fetch a recently deleted object by its key and sequence number.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return The cached Blob if found in the deleted cache; nullopt otherwise
     */
    std::optional<Blob>
    getDeleted(xrpl::uint256 const& key, uint32_t seq) const override;

    /**
     * @brief Gets a cached successor; always nullopt while the cache is not full.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return The cached successor if found; nullopt otherwise
     */
    std::optional<LedgerObject>
    getSuccessor(xrpl::uint256 const& key, uint32_t seq) const override;

    /**
     * @brief Gets a cached predecessor; always nullopt while the cache is not full.
     *
     * @param key The key to fetch for
     * @param seq The sequence to fetch for
     * @return The cached predecessor if found; nullopt otherwise
     */
    std::optional<LedgerObject>
    getPredecessor(xrpl::uint256 const& key, uint32_t seq) const override;

    /**
     * @brief Disables the cache.
     */
    void
    setDisabled() override;

    /**
     * @brief Check whether the cache is disabled.
     *
     * @return true if the cache is disabled; false otherwise
     */
    bool
    isDisabled() const override;

    /**
     * @brief Mark the cache as fully loaded and clear the currently-loading flag; no-op if the
     * cache is disabled.
     */
    void
    setFull() override;

    /**
     * @brief Get the latest ledger sequence held by the cache.
     *
     * @return The latest ledger sequence for which cache is available
     */
    uint32_t
    latestLedgerSequence() const override;

    /**
     * @brief Check whether the cache is fully loaded.
     *
     * @return true if the cache has all data for the most recent ledger; false otherwise
     */
    bool
    isFull() const override;

    /**
     * @brief Get the number of objects in the cache.
     *
     * @return The total size of the cache
     */
    size_t
    size() const override;

    /**
     * @brief Get the object lookup hit rate.
     *
     * @return The ratio of cache hits to requests for ledger objects
     */
    float
    getObjectHitRate() const override;

    /**
     * @brief Get the successor lookup hit rate.
     *
     * @return The ratio of cache hits to requests for successor keys
     */
    float
    getSuccessorHitRate() const override;

    /**
     * @brief Block until the cache contains the given sequence; returns immediately if the
     * cache is disabled.
     *
     * @param seq The sequence to wait for
     */
    void
    waitUntilCacheContainsSeq(uint32_t seq) override;

    /**
     * @brief Save the cache to file.
     *
     * @param path The file path to save the cache to
     * @return An error as a string if any
     */
    std::expected<void, std::string>
    saveToFile(std::string const& path) const override;

    /**
     * @brief Load the cache from file.
     *
     * @param path The file path to load data from
     * @param minLatestSequence The minimum allowed value of the latestLedgerSequence in cache file
     * @return An error as a string if any
     */
    std::expected<void, std::string>
    loadFromFile(std::string const& path, uint32_t minLatestSequence) override;

    /**
     * @brief Mark the cache as currently loading from the backend.
     */
    void
    startLoading() override;

    /**
     * @brief Check whether the cache is currently being loaded from the backend.
     *
     * @return true if startLoading() has been called and setFull() has not yet been called
     */
    [[nodiscard]] bool
    isCurrentlyLoading() const override;
};

}  // namespace data
