#pragma once

#include "util/Mutex.hpp"

#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <optional>
#include <shared_mutex>

namespace data {

/**
 * @brief A simple cache holding one `ripple::LedgerHeader` to reduce DB lookups.
 *
 * Used internally by backend implementations. When a ledger header is
 * fetched via `FetchLedgerBySeq` (often triggered by RPC commands),
 * the result can be stored here. Subsequent requests for the same ledger
 * sequence can proceed to retrieve the header from this cache, avoiding unnecessary
 * database reads and improving performance.
 */
class FetchLedgerCache {
public:
    FetchLedgerCache();

    /**
     * @brief Struct to store ledger header cache entry and the sequence it belongs to
     */
    struct CacheEntry {
        ripple::LedgerHeader ledger;
        uint32_t seq{};

        /**
         * @brief Comparing CacheEntry. Used in testing for EXPECT_CALL
         *
         * @param other The other cacheEntry to compare
         * @return true if two CacheEntry is the same, false otherwise
         */
        bool
        operator==(CacheEntry const& other) const
        {
            return ledger.hash == other.ledger.hash && seq == other.seq;
        }
    };

    /**
     * @brief Put CacheEntry into thread-safe container
     *
     * @param cacheEntry The Cache to store into thread-safe container.
     */
    void
    put(CacheEntry const& cacheEntry);

    /**
     * @brief Read CacheEntry from thread-safe container.
     *
     * @return Optional CacheEntry, depending on if it exists in thread-safe container or not.
     */
    std::optional<CacheEntry>
    get() const;

private:
    mutable util::Mutex<std::optional<CacheEntry>, std::shared_mutex> mutex_;
};

}  // namespace data
