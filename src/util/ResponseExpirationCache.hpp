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
        [[nodiscard]] std::optional<boost::json::object>
        get() const;

        /**
         * @brief Get the last time the cache was updated
         *
         * @return The last time the cache was updated
         */
        [[nodiscard]] std::chrono::steady_clock::time_point
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

    /**
     * @brief Check whether a request is "bare" (carries no response-customizing params).
     *
     * A request is bare iff every key it contains is one of: "command", "method", "id".
     * Any other key (e.g. "api_version", "hash", "counters", "ledger_index") makes the request
     * non-bare, and the cache must be bypassed for it.
     *
     * @param request The request object to inspect
     * @return true if the request is bare; false otherwise
     */
    static bool
    isBareRequest(boost::json::object const& request);

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
     * Only bare requests (those carrying no response-customizing params beyond "command", "method",
     * and "id") are served from the cache. Non-bare requests always return std::nullopt.
     *
     * @param cmd The command to get the response for
     * @param request The full request object; used to determine whether the request is bare
     * @return The cached response if available and not expired, or std::nullopt otherwise
     */
    [[nodiscard]] std::optional<boost::json::object>
    get(std::string const& cmd, boost::json::object const& request) const;

    /**
     * @brief Put a response into the cache if the request should be cached
     *
     * Only bare requests (those carrying no response-customizing params beyond "command", "method",
     * and "id") are stored in the cache. Non-bare requests are silently ignored.
     *
     * @param cmd The command to store the response for
     * @param request The full request object; used to determine whether the request is bare
     * @param response The response to store
     */
    void
    put(std::string const& cmd,
        boost::json::object const& request,
        boost::json::object const& response);

    /**
     * @brief Invalidate all entries in the cache
     */
    void
    invalidate();
};

}  // namespace util
