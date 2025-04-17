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

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

namespace util {

ResponseExpirationCache::ResponseExpirationCache(
    std::chrono::steady_clock::duration cacheTimeout,
    std::unordered_set<std::string> const& cmds
)
    : cacheTimeout_(cacheTimeout)
{
    for (auto const& command : cmds) {
        cache_.emplace(command, std::make_unique<CacheEntry>());
    }
}

bool
ResponseExpirationCache::shouldCache(std::string const& cmd)
{
    return cache_.contains(cmd);
}

std::expected<boost::json::object, ResponseExpirationCache::Error>
ResponseExpirationCache::getOrUpdate(
    boost::asio::yield_context yield,
    std::string const& cmd,
    Updater updater,
    Verifier verifier
)
{
    auto it = cache_.find(cmd);
    ASSERT(it != cache_.end(), "Can't get a value which is not in the cache");

    auto& entry = it->second;
    {
        auto result = entry->asyncGet(yield, updater, verifier);
        if (not result.has_value()) {
            return std::unexpected{std::move(result).error()};
        }
        if (std::chrono::steady_clock::now() - result->lastUpdated < cacheTimeout_) {
            return std::move(result)->response;
        }
    }

    // Force update due to cache timeout
    auto result = entry->update(yield, std::move(updater), std::move(verifier));
    if (not result.has_value()) {
        return std::unexpected{std::move(result).error()};
    }
    return std::move(result)->response;
}

void
ResponseExpirationCache::invalidate()
{
    for (auto& [_, entry] : cache_) {
        entry->invalidate();
    }
}

}  // namespace util
