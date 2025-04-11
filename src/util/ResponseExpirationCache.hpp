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
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace util {

/**
 * @brief Cache of requests' responses with TTL support and configurable cachable commands
 *
 * This class implements a time-based expiration cache for RPC responses. It allows
 * caching responses for specified commands and automatically invalidates them after
 * a configured timeout period. The cache uses BlockingCache internally to handle
 * concurrent access and updates.
 */
class ResponseExpirationCache {
public:
    /**
     * @brief A data structure to store a cache entry with its timestamp
     */
    struct EntryData {
        std::chrono::steady_clock::time_point lastUpdated;  ///< When the entry was last updated
        boost::json::object response;                       ///< The cached response data
    };

    /**
     * @brief A data structure to represent errors that can occur during an update of the cache
     */
    struct Error {
        rpc::Status status;           ///< The status code and message of the error
        boost::json::array warnings;  ///< Any warnings related to the request

        bool
        operator==(Error const&) const = default;
    };

    using CacheEntry = util::BlockingCache<EntryData, Error>;

private:
    std::chrono::steady_clock::duration cacheTimeout_;
    std::unordered_map<std::string, std::unique_ptr<CacheEntry>> cache_;

public:
    /**
     * @brief Construct a new ResponseExpirationCache object
     *
     * @param cacheTimeout The time period after which cached entries expire
     * @param cmds The commands that should be cached (requests for other commands won't be cached)
     */
    ResponseExpirationCache(
        std::chrono::steady_clock::duration cacheTimeout,
        std::unordered_set<std::string> const& cmds
    );

    /**
     * @brief Check if the given command should be cached
     *
     * @param cmd The command to check
     * @return true if the command should be cached, false otherwise
     */
    bool
    shouldCache(std::string const& cmd);

    using Updater = CacheEntry::Updater;
    using Verifier = CacheEntry::Verifier;

    /**
     * @brief Get a cached response or update the cache if necessary
     *
     * This method returns a cached response if it exists and hasn't expired.
     * If the cache entry is expired or doesn't exist, it calls the updater to
     * generate a new value. If multiple coroutines request the same entry
     * simultaneously, only one updater will be called while others wait.
     *
     * @note cmd must be one of the commands that are cached. There is an ASSERT() inside the function
     *
     * @param yield Asio yield context for coroutine suspension
     * @param cmd The command to get the response for
     * @param updater Function to generate the response if not in cache or expired
     * @param verifier Function to validate if a response should be cached
     * @return The cached or newly generated response, or an error
     */
    [[nodiscard]] std::expected<boost::json::object, Error>
    getOrUpdate(boost::asio::yield_context yield, std::string const& cmd, Updater updater, Verifier verifier);

    /**
     * @brief Invalidate all entries in the cache
     *
     * This causes all cached entries to be cleared, forcing the next access
     * to generate new responses.
     */
    void
    invalidate();
};
}  // namespace util
