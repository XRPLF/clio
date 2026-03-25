#pragma once

#include <boost/json/object.hpp>

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
     * @param request The request as json object
     * @return If the total sums up to a value equal or larger than maxRequestCount_
     * the operation is no longer allowed and false is returned; true is
     * returned otherwise.
     */
    [[maybe_unused]] virtual bool
    request(std::string const& ip, boost::json::object const& request) = 0;
};

}  // namespace web::dosguard
