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
#include "web/dosguard/WhitelistHandlerInterface.hpp"

#include <boost/iterator/transform_iterator.hpp>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

using namespace util::config;

namespace web::dosguard {

DOSGuard::DOSGuard(ClioConfigDefinition const& config, WhitelistHandlerInterface const& whitelistHandler)
    : whitelistHandler_{std::cref(whitelistHandler)}
    , maxFetches_{config.getValue("dos_guard.max_fetches").asIntType<uint32_t>()}
    , maxConnCount_{config.getValue("dos_guard.max_connections").asIntType<uint32_t>()}
    , maxRequestCount_{config.getValue("dos_guard.max_requests").asIntType<uint32_t>()}
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
        std::scoped_lock const lck(mtx_);
        if (ipState_.find(ip) != ipState_.end()) {
            auto [transferedByte, requests] = ipState_.at(ip);
            if (transferedByte > maxFetches_ || requests > maxRequestCount_) {
                LOG(log_.warn()) << "Dosguard: Client surpassed the rate limit. ip = " << ip
                                 << " Transfered Byte: " << transferedByte << "; Requests: " << requests;
                return false;
            }
        }
        auto it = ipConnCount_.find(ip);
        if (it != ipConnCount_.end()) {
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
    std::scoped_lock const lck{mtx_};
    ipConnCount_[ip]++;
}

void
DOSGuard::decrement(std::string const& ip) noexcept
{
    if (whitelistHandler_.get().isWhiteListed(ip))
        return;
    std::scoped_lock const lck{mtx_};
    ASSERT(ipConnCount_[ip] > 0, "Connection count for ip {} can't be 0", ip);
    ipConnCount_[ip]--;
    if (ipConnCount_[ip] == 0)
        ipConnCount_.erase(ip);
}

[[maybe_unused]] bool
DOSGuard::add(std::string const& ip, uint32_t numObjects) noexcept
{
    if (whitelistHandler_.get().isWhiteListed(ip))
        return true;

    {
        std::scoped_lock const lck(mtx_);
        ipState_[ip].transferedByte += numObjects;
    }

    return isOk(ip);
}

[[maybe_unused]] bool
DOSGuard::request(std::string const& ip) noexcept
{
    if (whitelistHandler_.get().isWhiteListed(ip))
        return true;

    {
        std::scoped_lock const lck(mtx_);
        ipState_[ip].requestsCount++;
    }

    return isOk(ip);
}

void
DOSGuard::clear() noexcept
{
    std::scoped_lock const lck(mtx_);
    ipState_.clear();
}

[[nodiscard]] std::unordered_set<std::string>
DOSGuard::getWhitelist(ClioConfigDefinition const& config)
{
    std::unordered_set<std::string> ips;
    auto const whitelist = config.getArray("dos_guard.whitelist");

    if (whitelist.valueAt(0).hasValue()) {
        for (auto it = whitelist.begin<ValueView>(); it != whitelist.end<ValueView>(); ++it) {
            ips.insert((*it).asString());
        }
    }
    return ips;
}

}  // namespace web::dosguard
