//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "util/Assert.h"
#include "util/config/Config.h"
#include "util/log/Logger.h"
#include "web/IntervalSweepHandler.h"
#include "web/WhitelistHandler.h"

#include <boost/asio.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/system/error_code.hpp>

#include <cstdint>
#include <functional>
#include <iterator>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace web {

/**
 * @brief The interface of a denial of service guard.
 */
class BaseDOSGuard {
public:
    virtual ~BaseDOSGuard() = default;

    /**
     * @brief Clears implementation-defined counters.
     */
    virtual void
    clear() noexcept = 0;
};

/**
 * @brief A simple denial of service guard used for rate limiting.
 *
 * @tparam WhitelistHandlerType The type of the whitelist handler
 * @tparam SweepHandlerType The type of the sweep handler
 */
template <typename WhitelistHandlerType, typename SweepHandlerType>
class BasicDOSGuard : public BaseDOSGuard {
    // Accumulated state per IP, state will be reset accordingly
    struct ClientState {
        // accumulated transfered byte
        std::uint32_t transferedByte = 0;
        // accumulated served requests count
        std::uint32_t requestsCount = 0;
    };

    mutable std::mutex mtx_;
    // accumulated states map
    std::unordered_map<std::string, ClientState> ipState_;
    std::unordered_map<std::string, std::uint32_t> ipConnCount_;
    std::reference_wrapper<WhitelistHandlerType const> whitelistHandler_;

    std::uint32_t const maxFetches_;
    std::uint32_t const maxConnCount_;
    std::uint32_t const maxRequestCount_;
    util::Logger log_{"RPC"};

public:
    static constexpr std::uint32_t DEFAULT_MAX_FETCHES = 1000'000u;
    static constexpr std::uint32_t DEFAULT_MAX_CONNECTIONS = 20u;
    static constexpr std::uint32_t DEFAULT_MAX_REQUESTS = 20u;
    /**
     * @brief Constructs a new DOS guard.
     *
     * @param config Clio config
     * @param whitelistHandler Whitelist handler that checks whitelist for IP addresses
     * @param sweepHandler Sweep handler that implements the sweeping behaviour
     */
    BasicDOSGuard(
        util::Config const& config,
        WhitelistHandlerType const& whitelistHandler,
        SweepHandlerType& sweepHandler
    )
        : whitelistHandler_{std::cref(whitelistHandler)}
        , maxFetches_{config.valueOr("dos_guard.max_fetches", DEFAULT_MAX_FETCHES)}
        , maxConnCount_{config.valueOr("dos_guard.max_connections", DEFAULT_MAX_CONNECTIONS)}
        , maxRequestCount_{config.valueOr("dos_guard.max_requests", DEFAULT_MAX_REQUESTS)}
    {
        sweepHandler.setup(this);
    }

    /**
     * @brief Check whether an ip address is in the whitelist or not.
     *
     * @param ip The ip address to check
     * @return true
     * @return false
     */
    [[nodiscard]] bool
    isWhiteListed(std::string_view const ip) const noexcept
    {
        return whitelistHandler_.get().isWhiteListed(ip);
    }

    /**
     * @brief Check whether an ip address is currently rate limited or not.
     *
     * @param ip The ip address to check
     * @return true If not rate limited
     * @return false If rate limited and the request should not be processed
     */
    [[nodiscard]] bool
    isOk(std::string const& ip) const noexcept
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

    /**
     * @brief Increment connection count for the given ip address.
     *
     * @param ip
     */
    void
    increment(std::string const& ip) noexcept
    {
        if (whitelistHandler_.get().isWhiteListed(ip))
            return;
        std::scoped_lock const lck{mtx_};
        ipConnCount_[ip]++;
    }

    /**
     * @brief Decrement connection count for the given ip address.
     *
     * @param ip
     */
    void
    decrement(std::string const& ip) noexcept
    {
        if (whitelistHandler_.get().isWhiteListed(ip))
            return;
        std::scoped_lock const lck{mtx_};
        ASSERT(ipConnCount_[ip] > 0, "Connection count for ip {} can't be 0", ip);
        ipConnCount_[ip]--;
        if (ipConnCount_[ip] == 0)
            ipConnCount_.erase(ip);
    }

    /**
     * @brief Adds numObjects of usage for the given ip address.
     *
     * If the total sums up to a value equal or larger than maxFetches_
     * the operation is no longer allowed and false is returned; true is
     * returned otherwise.
     *
     * @param ip
     * @param numObjects
     * @return true
     * @return false
     */
    [[maybe_unused]] bool
    add(std::string const& ip, uint32_t numObjects) noexcept
    {
        if (whitelistHandler_.get().isWhiteListed(ip))
            return true;

        {
            std::scoped_lock const lck(mtx_);
            ipState_[ip].transferedByte += numObjects;
        }

        return isOk(ip);
    }

    /**
     * @brief Adds one request for the given ip address.
     *
     * If the total sums up to a value equal or larger than maxRequestCount_
     * the operation is no longer allowed and false is returned; true is
     * returned otherwise.
     *
     * @param ip
     * @return true
     * @return false
     */
    [[maybe_unused]] bool
    request(std::string const& ip) noexcept
    {
        if (whitelistHandler_.get().isWhiteListed(ip))
            return true;

        {
            std::scoped_lock const lck(mtx_);
            ipState_[ip].requestsCount++;
        }

        return isOk(ip);
    }

    /**
     * @brief Instantly clears all fetch counters added by @see add(std::string const&, uint32_t).
     */
    void
    clear() noexcept override
    {
        std::scoped_lock const lck(mtx_);
        ipState_.clear();
    }

private:
    [[nodiscard]] std::unordered_set<std::string>
    getWhitelist(util::Config const& config) const
    {
        using T = std::unordered_set<std::string> const;
        auto whitelist = config.arrayOr("dos_guard.whitelist", {});
        auto const transform = [](auto const& elem) { return elem.template value<std::string>(); };
        return T{
            boost::transform_iterator(std::begin(whitelist), transform),
            boost::transform_iterator(std::end(whitelist), transform)
        };
    }
};

using DOSGuard = BasicDOSGuard<web::WhitelistHandler, web::IntervalSweepHandler>;

}  // namespace web
