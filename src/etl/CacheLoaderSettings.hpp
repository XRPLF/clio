#pragma once

#include "util/config/ConfigDefinition.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace etl {

/**
 * @brief Settings for the cache loader
 */
struct CacheLoaderSettings {
    /** @brief Ways to load the cache */
    enum class LoadStyle { ASYNC, SYNC, NONE };

    /** @brief Settings for cache file operations */
    struct CacheFileSettings {
        std::string
            path; /**< path to the file to load cache from on start and save cache to on shutdown */
        uint32_t maxAge = 5000; /**< max difference between latest sequence in cache file and DB */

        auto
        operator<=>(CacheFileSettings const&) const = default;
    };

    size_t numCacheDiffs = 32;   /**< number of diffs to use to generate cursors */
    size_t numCacheMarkers = 48; /**< number of markers to use at one time to traverse the ledger */
    size_t cachePageFetchSize =
        512;               /**< number of ledger objects to fetch concurrently per marker */
    size_t numThreads = 2; /**< number of threads to use for loading cache */
    size_t numCacheCursorsFromDiff = 0;    /**< number of cursors to fetch from diff */
    size_t numCacheCursorsFromAccount = 0; /**< number of cursors to fetch from account_tx */

    LoadStyle loadStyle = LoadStyle::ASYNC; /**< how to load the cache */
    std::optional<CacheFileSettings>
        cacheFileSettings; /**< optional settings for cache file operations */

    auto
    operator<=>(CacheLoaderSettings const&) const = default;

    /** @returns True if the load style is SYNC; false otherwise */
    [[nodiscard]] bool
    isSync() const;

    /** @returns True if the load style is ASYNC; false otherwise */
    [[nodiscard]] bool
    isAsync() const;

    /** @returns True if the cache is disabled; false otherwise */
    [[nodiscard]] bool
    isDisabled() const;
};

/**
 * @brief Create a CacheLoaderSettings object from a Config object
 *
 * @param config The configuration object
 * @returns The CacheLoaderSettings object
 */
[[nodiscard]] CacheLoaderSettings
makeCacheLoaderSettings(util::config::ClioConfigDefinition const& config);

}  // namespace etl
