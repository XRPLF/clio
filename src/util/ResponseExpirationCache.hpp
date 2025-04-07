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

#pragma once

#include "rpc/Errors.hpp"
#include "util/BlockingCache.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace util {

/**
 * @brief Cache of requests' responses with TTL support and configurable cachable commands
 */
class ResponseExpirationCache {
public:
    /**
     * @brief A class to store a cache entry.
     */
    struct EntryData {
        std::chrono::steady_clock::time_point lastUpdated;
        boost::json::object response;
    };

    using CacheEntry = util::BlockingCache<EntryData, rpc::CombinedError>;

private:
    std::chrono::steady_clock::duration cacheTimeout_;
    std::unordered_map<std::string, CacheEntry> cache_;

public:
    /**
     * @brief Construct a new Cache object
     *
     * @param cacheTimeout The time for cache entries to expire
     * @param cmds The commands that should be cached
     */
    ResponseExpirationCache(
        std::chrono::steady_clock::duration cacheTimeout,
        std::unordered_set<std::string> const& cmds
    );

    bool
    shouldCache(std::string const& cmd);

    using Updater = CacheEntry::Updater;
    using Verifier = CacheEntry::Verifier;

    /**
     * @brief Get a response from the cache
     *
     * @param cmd The command to get the response for
     * @return The response if it exists or std::nullopt otherwise
     */
    [[nodiscard]] std::expected<boost::json::object, rpc::CombinedError>
    getOrUpdate(boost::asio::yield_context yield, std::string const& cmd, Updater updater, Verifier verifier);

    /**
     * @brief Invalidate all entries in the cache
     */
    void
    invalidate();
};
}  // namespace util
