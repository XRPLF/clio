#include "util/ResponseExpirationCache.hpp"

#include "util/Assert.hpp"

#include <boost/json/object.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
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

bool
ResponseExpirationCache::isBareRequest(boost::json::object const& request)
{
    // Keys that identify the command or are safe to ignore for caching purposes.
    //
    // "command" and "method" merely name the RPC command — they do not affect response content.
    //
    // "id" is safe to ignore because the web layer re-applies the current request's id to the
    // response AFTER the cache lookup (see src/web/RPCServerHandler.hpp), so a cached body never
    // leaks a stale id back to the client.  Ignoring "id" is also necessary for WebSocket
    // requests, which almost always carry an id and would otherwise never benefit from caching.
    //
    // "api_version" is intentionally NOT in this ignore-set: it can change the response body
    // content (not just an echoed field), so requests carrying it must bypass the cache entirely.
    static constexpr auto kIgnoredKeys =
        std::to_array<std::string_view>({"command", "method", "id"});

    for (auto const& kv : request) {
        std::string_view const key{kv.key()};
        if (not std::ranges::contains(kIgnoredKeys, key))
            return false;
    }
    return true;
}

std::optional<boost::json::object>
ResponseExpirationCache::get(std::string const& cmd, boost::json::object const& request) const
{
    if (not isBareRequest(request))
        return std::nullopt;

    auto it = cache_.find(cmd);
    if (it == cache_.end())
        return std::nullopt;

    auto const& entry = it->second.lock<std::shared_lock>();
    if (std::chrono::steady_clock::now() - entry->lastUpdated() > cacheTimeout_)
        return std::nullopt;

    return entry->get();
}

void
ResponseExpirationCache::put(
    std::string const& cmd,
    boost::json::object const& request,
    boost::json::object const& response
)
{
    if (not isBareRequest(request))
        return;

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
