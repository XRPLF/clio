#include "util/ResponseExpirationCache.hpp"

#include "util/Assert.hpp"

#include <boost/json/object.hpp>

#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <utility>

namespace util {

void
ResponseExpirationCache::Entry::put(boost::json::object response)
{
    response_ = std::move(response);
    lastUpdated_ = std::chrono::steady_clock::now();
}

std::optional<boost::json::object>
ResponseExpirationCache::Entry::get() const
{
    return response_;
}

std::chrono::steady_clock::time_point
ResponseExpirationCache::Entry::lastUpdated() const
{
    return lastUpdated_;
}

void
ResponseExpirationCache::Entry::invalidate()
{
    response_.reset();
}

bool
ResponseExpirationCache::shouldCache(std::string const& cmd)
{
    return cache_.contains(cmd);
}

std::optional<boost::json::object>
ResponseExpirationCache::get(std::string const& cmd) const
{
    auto it = cache_.find(cmd);
    if (it == cache_.end())
        return std::nullopt;

    auto const& entry = it->second.lock<std::shared_lock>();
    if (std::chrono::steady_clock::now() - entry->lastUpdated() > cacheTimeout_)
        return std::nullopt;

    return entry->get();
}

void
ResponseExpirationCache::put(std::string const& cmd, boost::json::object const& response)
{
    if (not shouldCache(cmd))
        return;

    ASSERT(cache_.contains(cmd), "Command is not in the cache: {}", cmd);

    auto entry = cache_[cmd].lock<std::unique_lock>();
    entry->put(response);
}

void
ResponseExpirationCache::invalidate()
{
    for (auto& [_, entry] : cache_) {
        auto entryLock = entry.lock<std::unique_lock>();
        entryLock->invalidate();
    }
}

}  // namespace util
