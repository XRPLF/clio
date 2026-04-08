#pragma once

#include "util/Mutex.hpp"

#include <boost/json/object.hpp>

#include <chrono>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace util {

/**
 * @brief Cache of requests' responses with TTL support and configurable cacheable commands
 */
class ResponseExpirationCache {
    /**
     * @brief A class to store a cache entry.
     */
    class Entry {
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

    std::chrono::steady_clock::duration cacheTimeout_;
    std::unordered_map<std::string, util::Mutex<Entry, std::shared_mutex>> cache_;

    bool
    shouldCache(std::string const& cmd);

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
    )
        : cacheTimeout_(cacheTimeout)
    {
        for (auto const& command : cmds) {
            cache_.emplace(command, Entry{});
        }
    }

    /**
     * @brief Get a response from the cache
     *
     * @param cmd The command to get the response for
     * @return The response if it exists or std::nullopt otherwise
     */
    [[nodiscard]] std::optional<boost::json::object>
    get(std::string const& cmd) const;

    /**
     * @brief Put a response into the cache if the request should be cached
     *
     * @param cmd The command to store the response for
     * @param response The response to store
     */
    void
    put(std::string const& cmd, boost::json::object const& response);

    /**
     * @brief Invalidate all entries in the cache
     */
    void
    invalidate();
};

}  // namespace util
