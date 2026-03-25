#include "data/LedgerHeaderCache.hpp"

#include "util/Mutex.hpp"

#include <mutex>
#include <optional>
#include <shared_mutex>

namespace data {

FetchLedgerCache::FetchLedgerCache() = default;

void
FetchLedgerCache::put(CacheEntry const& cacheEntry)
{
    auto lock = mutex_.lock<std::unique_lock>();
    *lock = cacheEntry;
}

std::optional<FetchLedgerCache::CacheEntry>
FetchLedgerCache::get() const
{
    auto const lock = mutex_.lock<std::shared_lock>();
    return lock.get();
}

}  // namespace data
