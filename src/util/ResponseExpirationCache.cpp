//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/ResponseExpirationCache.hpp"

#include "util/Assert.hpp"

#include <boost/json/object.hpp>

#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>

namespace util {

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
