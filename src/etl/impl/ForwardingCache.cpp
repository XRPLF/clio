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

#include "etl/impl/ForwardingCache.hpp"

#include <boost/json/fwd.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_set>
#include <utility>

namespace etl::impl {

namespace {

std::optional<std::string>
getMethod(boost::json::object const& request)
{
    if (not request.contains("method")) {
        return std::nullopt;
    }
    return boost::json::value_to<std::string>(request.at("method"));
}

}  // namespace

void
CacheEntry::put(boost::json::object response)
{
    response_ = std::move(response);
    lastUpdated_ = std::chrono::steady_clock::now();
}

std::optional<boost::json::object>
CacheEntry::get() const
{
    return response_;
}

std::chrono::steady_clock::time_point
CacheEntry::lastUpdated() const
{
    return lastUpdated_;
}

void
CacheEntry::invalidate()
{
    response_.reset();
}

std::unordered_set<std::string> const
    ForwardingCache::CACHEABLE_METHODS{"server_info", "server_state", "server_definitions", "fee", "ledger_closed"};

ForwardingCache::ForwardingCache(std::chrono::steady_clock::duration const cacheTimeout) : cacheTimeout_{cacheTimeout}
{
}

bool
ForwardingCache::shouldCache(boost::json::object const& request)
{
    auto const method = getMethod(request);
    return method.has_value() and CACHEABLE_METHODS.contains(*method);
}

std::optional<boost::json::object>
ForwardingCache::get(boost::json::object const& request) const
{
    auto const method = getMethod(request);

    if (not method.has_value()) {
        return std::nullopt;
    }

    auto it = cache_.find(*method);
    if (it == cache_.end())
        return std::nullopt;

    auto const& entry = it->second.lock<std::shared_lock>();
    if (std::chrono::steady_clock::now() - entry->lastUpdated() > cacheTimeout_)
        return std::nullopt;

    return entry->get();
}

void
ForwardingCache::put(boost::json::object const& request, boost::json::object const& response)
{
    auto const method = getMethod(request);
    if (not method.has_value()) {
        return;
    }
    auto entry = cache_[*method].lock<std::unique_lock>();
    entry->put(response);
}

void
ForwardingCache::invalidate()
{
    for (auto& [_, entry] : cache_) {
        auto entryLock = entry.lock<std::unique_lock>();
        entryLock->invalidate();
    }
}

}  // namespace etl::impl
