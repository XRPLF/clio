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

#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"
#include "web/dosguard/WhitelistHandlerInterface.hpp"

#include <boost/asio.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/system/error_code.hpp>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace web::dosguard {

/**
 * @brief A simple denial of service guard used for rate limiting.
 *
 * @tparam WhitelistHandlerType The type of the whitelist handler
 */
class DOSGuard : public DOSGuardInterface {
    /**
     * @brief Accumulated state per IP, state will be reset accordingly
     */
    struct ClientState {
        std::uint32_t transferedByte = 0; /**< Accumulated transferred byte */
        std::uint32_t requestsCount = 0;  /**< Accumulated served requests count */
    };

    mutable std::mutex mtx_;
    std::unordered_map<std::string, ClientState> ipState_;
    std::unordered_map<std::string, std::uint32_t> ipConnCount_;
    std::reference_wrapper<WhitelistHandlerInterface const> whitelistHandler_;

    std::uint32_t const maxFetches_;
    std::uint32_t const maxConnCount_;
    std::uint32_t const maxRequestCount_;
    util::Logger log_{"RPC"};

public:
    /**
     * @brief Constructs a new DOS guard.
     *
     * @param config Clio config
     * @param whitelistHandler Whitelist handler that checks whitelist for IP addresses
     */
    DOSGuard(util::config::ClioConfigDefinition const& config, WhitelistHandlerInterface const& whitelistHandler);

    /**
     * @brief Check whether an ip address is in the whitelist or not.
     *
     * @param ip The ip address to check
     * @return true
     * @return false
     */
    [[nodiscard]] bool
    isWhiteListed(std::string_view const ip) const noexcept override;

    /**
     * @brief Check whether an ip address is currently rate limited or not.
     *
     * @param ip The ip address to check
     * @return true If not rate limited
     * @return false If rate limited and the request should not be processed
     */
    [[nodiscard]] bool
    isOk(std::string const& ip) const noexcept override;

    /**
     * @brief Increment connection count for the given ip address.
     *
     * @param ip
     */
    void
    increment(std::string const& ip) noexcept override;

    /**
     * @brief Decrement connection count for the given ip address.
     *
     * @param ip
     */
    void
    decrement(std::string const& ip) noexcept override;

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
    add(std::string const& ip, uint32_t numObjects) noexcept override;

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
    request(std::string const& ip) noexcept override;

    /**
     * @brief Instantly clears all fetch counters added by @see add(std::string const&, uint32_t).
     */
    void
    clear() noexcept override;

private:
    [[nodiscard]] static std::unordered_set<std::string>
    getWhitelist(util::config::ClioConfigDefinition const& config);
};

}  // namespace web::dosguard
