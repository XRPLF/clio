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

#include <cstdint>
#include <string>
#include <string_view>
namespace web::dosguard {

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
 * @brief The interface of a denial of service guard.
 */
class DOSGuardInterface : public BaseDOSGuard {
public:
    /**
     * @brief Check whether an ip address is in the whitelist or not.
     *
     * @param ip The ip address to check
     * @return true
     * @return false
     */
    [[nodiscard]] virtual bool
    isWhiteListed(std::string_view const ip) const noexcept = 0;

    /**
     * @brief Check whether an ip address is currently rate limited or not.
     *
     * @param ip The ip address to check
     * @return true If not rate limited
     * @return false If rate limited and the request should not be processed
     */
    [[nodiscard]] virtual bool
    isOk(std::string const& ip) const noexcept = 0;

    /**
     * @brief Increment connection count for the given ip address.
     *
     * @param ip
     */
    virtual void
    increment(std::string const& ip) noexcept = 0;

    /**
     * @brief Decrement connection count for the given ip address.
     *
     * @param ip
     */
    virtual void
    decrement(std::string const& ip) noexcept = 0;

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
    [[maybe_unused]] virtual bool
    add(std::string const& ip, uint32_t numObjects) noexcept = 0;

    /**
     * @brief Adds one request for the given ip address.
     *
     *
     * @param ip
     * @return If the total sums up to a value equal or larger than maxRequestCount_
     * the operation is no longer allowed and false is returned; true is
     * returned otherwise.
     */
    [[maybe_unused]] virtual bool
    request(std::string const& ip) noexcept = 0;
};

}  // namespace web::dosguard
