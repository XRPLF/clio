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

#include "util/Mutex.hpp"

#include <boost/json/object.hpp>

#include <chrono>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace etl::impl {

/**
 * @brief A class to store a cache entry.
 */
class CacheEntry {
    std::chrono::steady_clock::time_point lastUpdated_;
    std::optional<boost::json::object> response_;

public:
    /**
     * @brief Put a response into the cache
     *
     * @param response The response to store
     */
    void
    put(boost::json::object response);

    /**
     * @brief Get the response from the cache
     *
     * @return The response
     */
    std::optional<boost::json::object>
    get() const;

    /**
     * @brief Get the last time the cache was updated
     *
     * @return The last time the cache was updated
     */
    std::chrono::steady_clock::time_point
    lastUpdated() const;

    /**
     * @brief Invalidate the cache entry
     */
    void
    invalidate();
};

class ForwardingCache {
    std::chrono::steady_clock::duration cacheTimeout_;
    std::unordered_map<std::string, util::Mutex<CacheEntry, std::shared_mutex>> cache_;

public:
    static std::unordered_set<std::string> const CACHEABLE_COMMANDS;

    ForwardingCache(std::chrono::steady_clock::duration cacheTimeout);

    static bool
    shouldCache(boost::json::object const& request);

    std::optional<boost::json::object>
    get(boost::json::object const& request) const;

    void
    put(boost::json::object const& request, boost::json::object const& response);

    void
    invalidate();

private:
    static std::optional<std::string>
    getCommand(boost::json::object const& request);
};

}  // namespace etl::impl
