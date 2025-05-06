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

#include "web/dosguard/DOSGuard.hpp"

#include "util/Assert.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ValueView.hpp"
#include "web/dosguard/WeightsInterface.hpp"
#include "web/dosguard/WhitelistHandlerInterface.hpp"

#include <boost/json/object.hpp>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

using namespace util::config;

namespace web::dosguard {

DOSGuard::DOSGuard(
    ClioConfigDefinition const& config,
    WhitelistHandlerInterface const& whitelistHandler,
    WeightsInterface const& weights
)
    : whitelistHandler_{std::cref(whitelistHandler)}
    , weights_(weights)
    , maxFetches_{config.get<uint32_t>("dos_guard.max_fetches")}
    , maxConnCount_{config.get<uint32_t>("dos_guard.max_connections")}
    , maxRequestCount_{config.get<uint32_t>("dos_guard.max_requests")}
{
}

[[nodiscard]] bool
DOSGuard::isWhiteListed(std::string_view const ip) const noexcept
{
    return whitelistHandler_.get().isWhiteListed(ip);
}

[[nodiscard]] bool
DOSGuard::isOk(std::string const& ip) const noexcept
{
    if (whitelistHandler_.get().isWhiteListed(ip))
        return true;

    {
        auto lock = mtx_.lock<std::scoped_lock>();
        if (auto const it = lock->ipState.find(ip); it != lock->ipState.end()) {
            auto const [transferredByte, requests] = it->second;
            if (transferredByte > maxFetches_ || requests > maxRequestCount_) {
                LOG(log_.warn()) << "Dosguard: Client surpassed the rate limit. ip = " << ip
                                 << " Transferred Byte: " << transferredByte << "; Requests: " << requests;
                return false;
            }
        }
        auto it = lock->ipConnCount.find(ip);
        if (it != lock->ipConnCount.end()) {
            if (it->second > maxConnCount_) {
                LOG(log_.warn()) << "Dosguard: Client surpassed the rate limit. ip = " << ip
                                 << " Concurrent connection: " << it->second;
                return false;
            }
        }
    }
    return true;
}

void
DOSGuard::increment(std::string const& ip) noexcept
{
    if (whitelistHandler_.get().isWhiteListed(ip))
        return;
    auto lock = mtx_.lock<std::scoped_lock>();
    lock->ipConnCount[ip]++;
}

void
DOSGuard::decrement(std::string const& ip) noexcept
{
    if (whitelistHandler_.get().isWhiteListed(ip))
        return;
    auto lock = mtx_.lock<std::scoped_lock>();
    ASSERT(lock->ipConnCount[ip] > 0, "Connection count for ip {} can't be 0", ip);
    lock->ipConnCount[ip]--;
    if (lock->ipConnCount[ip] == 0)
        lock->ipConnCount.erase(ip);
}

[[maybe_unused]] bool
DOSGuard::add(std::string const& ip, uint32_t numObjects) noexcept
{
    if (whitelistHandler_.get().isWhiteListed(ip))
        return true;

    {
        auto lock = mtx_.lock<std::scoped_lock>();
        lock->ipState[ip].transferredByte += numObjects;
    }

    return isOk(ip);
}

[[maybe_unused]] bool
DOSGuard::request(std::string const& ip, boost::json::object const& request)
{
    if (whitelistHandler_.get().isWhiteListed(ip))
        return true;

    auto const weight = weights_.get().requestWeight(request);

    {
        auto lock = mtx_.lock<std::scoped_lock>();
        lock->ipState[ip].requestsCount += weight;
    }

    return isOk(ip);
}

void
DOSGuard::clear() noexcept
{
    auto lock = mtx_.lock<std::scoped_lock>();
    lock->ipState.clear();
}

[[nodiscard]] std::unordered_set<std::string>
DOSGuard::getWhitelist(ClioConfigDefinition const& config)
{
    std::unordered_set<std::string> ips;
    auto const whitelist = config.getArray("dos_guard.whitelist");

    for (auto it = whitelist.begin<ValueView>(); it != whitelist.end<ValueView>(); ++it) {
        ips.insert((*it).asString());
    }

    return ips;
}

}  // namespace web::dosguard
