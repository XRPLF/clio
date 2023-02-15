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

#include <boost/asio.hpp>
#include <boost/iterator/transform_iterator.hpp>

#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <config/Config.h>

#include <iostream>
#include <string>
#include <vector>

namespace clio {

class BaseDOSGuard
{
public:
    virtual ~BaseDOSGuard() = default;

    virtual void
    clear() noexcept = 0;
};

/**
 * @brief A simple denial of service guard used for rate limiting.
 *
 * @tparam SweepHandler Type of the sweep handler
 */
template <typename SweepHandler>
class BasicDOSGuard : public BaseDOSGuard
{
    // Accumulated state per IP, state will be reset accordingly
    struct ClientState
    {
        // accumulated transfered byte
        std::uint32_t transferedByte = 0;
        // accumulated served requests count
        std::uint32_t requestsCount = 0;
    };

    mutable std::mutex mtx_;
    // accumulated states map
    std::unordered_map<std::string, ClientState> ipState_;
    std::unordered_map<std::string, std::uint32_t> ipConnCount_;
    std::unordered_set<std::string> const whitelist_;

    std::uint32_t const maxFetches_;
    std::uint32_t const maxConnCount_;
    std::uint32_t const maxRequestCount_;
    clio::Logger log_{"RPC"};

public:
    /**
     * @brief Checks if raw IPv4 address is valid
     *
     * @param ip Raw IPv4 address to check
     */
    [[nodiscard]] bool
    checkParsedIPv4Address(std::string const& ip) const
    {
        try
        {
            boost::asio::ip::address_v4::from_string(ip);
            return true;
        }

        catch (const std::exception& e)
        {
            return false;
        }
    }

    /**
     * @brief Checks if raw IPv6 address is valid
     *
     * @param ip Raw IPv6 address to check
     */
    [[nodiscard]] bool
    checkParsedIPv6Address(std::string const& ip) const
    {
        try
        {
            boost::asio::ip::address_v6::from_string(ip);
            return true;
        }

        catch (const std::exception& e)
        {
            return false;
        }
    }

    /**
     * @brief Checks if IPv4 address has a valid CIDR notation
     *
     * @param ip The IP address (IPv4)
     */
    [[nodiscard]] bool
    checkCIDRIPv4(std::string const& ip) const
    {
        if (ip.find("/") != std::string::npos)
        {
            std::string rawSubstring = ip.substr(ip.find("/"));
            // Check if there is an element at the end of the CIDR
            if (rawSubstring.length() > 1 && rawSubstring.length() <= 3)
            {
                int parsedCIDRNumber =
                    stoi(rawSubstring.substr(rawSubstring.find("/") + 1));

                if (parsedCIDRNumber >= 0 && parsedCIDRNumber <= 32)
                {
                    return true;
                }

                else
                {
                    return false;
                }
            }

            else
            {
                return false;
            }
        }

        else
        {
            return false;
        }
    }

    /**
     * @brief Checks if parsed byte has a valid CIDR notation
     *
     * @param ip The IP address (IPv6)
     */
    [[nodiscard]] bool
    checkCIDRIPv6(std::string const& ip) const
    {
        if (ip.find("/") != std::string::npos)
        {
            std::string rawSubstring = ip.substr(ip.find("/"));
            // Check if there is an element at the end of the CIDR
            if (rawSubstring.length() > 1 && rawSubstring.length() <= 4)
            {
                int parsedCIDRNumber =
                    stoi(rawSubstring.substr(rawSubstring.find("/") + 1));

                if (parsedCIDRNumber >= 0 && parsedCIDRNumber <= 128)
                {
                    return true;
                }

                else
                {
                    return false;
                }
            }

            else
            {
                return false;
            }
        }

        else
        {
            return false;
        }
    }

    /**
     * @brief Checks if IPv4 address belongs in specified subnet
     *
     * @param ip The ip being checked to see if it belongs in the subnet
     * @param subnetIPwCIDR The IPv4 subnet with attached CIDR
     */
    [[nodiscard]] bool
    isIPv4AddressInSubnet(
        std::string const& ip,
        std::string const& subnetIPwCIDR) const
    {
        // ip being checked does not have CIDR
        if (subnetIPwCIDR.find("/") != std::string::npos &&
            checkParsedIPv4Address(ip) == true)
        {
            boost::system::error_code subnetError;
            boost::asio::ip::network_v4 subnet =
                boost::asio::ip::make_network_v4(subnetIPwCIDR, subnetError);
            boost::asio::ip::address_v4_range subnetRangeHosts = subnet.hosts();

            if (subnetError)
            {
                throw std::invalid_argument(subnetError.message());
                return false;
            }

            boost::system::error_code error_code;
            boost::asio::ip::address addr =
                boost::asio::ip::make_address(ip, error_code);

            if (error_code)
            {
                throw std::invalid_argument(error_code.message());
                return false;
            }

            // Check if the address is in one of the hosts of subnet
            if (subnetRangeHosts.find(addr.to_v4()) != subnetRangeHosts.end())
            {
                return true;
            }
        }

        // Catch-All for incorrect cases
        return false;
    }

    /**
     * @brief Checks if IPv6 address belongs in specified subnet
     *
     * @param ip The ip being checked to see if it belongs in the subnet
     * @param subnetIPwCIDR The IPv6 subnet with attached CIDR
     */
    [[nodiscard]] bool
    isIPv6AddressInSubnet(
        std::string const& ip,
        std::string const& subnetIPwCIDR) const
    {
        // ip being checked does not have CIDR
        if (subnetIPwCIDR.find("/") != std::string::npos &&
            checkParsedIPv6Address(ip) == true)
        {
            boost::system::error_code subnetError;
            boost::asio::ip::network_v6 subnet =
                boost::asio::ip::make_network_v6(subnetIPwCIDR, subnetError);
            boost::asio::ip::address_v6_range subnetRangeHosts = subnet.hosts();

            if (subnetError)
            {
                throw std::invalid_argument(subnetError.message());
                return false;
            }

            boost::system::error_code error_code;
            boost::asio::ip::address addr =
                boost::asio::ip::make_address(ip, error_code);

            if (error_code)
            {
                throw std::invalid_argument(error_code.message());
                return false;
            }

            // Check if the address is in one of the hosts of subnet
            if (subnetRangeHosts.find(addr.to_v6()) != subnetRangeHosts.end())
            {
                return true;
            }
        }

        // Catch-All for incorrect cases
        return false;
    }

    /**
     * @brief Helper method that checks if an IP address is valid IPv4
     *
     * @param ip The IP address (explicit IPv4)
     */
    [[nodiscard]] bool
    isValidIPv4(std::string const& ip) const
    {
        // CASE #1: Checks specific to detection of CIDR for IPv4
        if (ip.find("/") != std::string::npos &&
            checkParsedIPv4Address(ip.substr(0, ip.find("/"))) == true &&
            checkCIDRIPv4(ip) == true)
        {
            return true;
        }

        // CASE #2: No CIDR, can check ip valid IP
        else if (ip.find("/") == std::string::npos)
        {
            return checkParsedIPv4Address(ip);
        }

        // Catch-All for incorrect cases
        return false;
    }

    /**
     * @brief Helper method checks if an IP address is a valid IPv6 address
     *
     * @param ip The IP address (explicit IPv6), with/without a CIDR
     */
    [[nodiscard]] bool
    isValidIPv6(std::string const& ip) const
    {
        // CASE #1: Checks specific to detection of CIDR for IPv6
        if (ip.find("/") != std::string::npos &&
            checkParsedIPv6Address(ip.substr(0, ip.find("/"))) == true &&
            checkCIDRIPv6(ip) == true)
        {
            return true;
        }

        // CASE #2: General check to determine if valid IPv6 if no CIDR
        else if (ip.find("/") == std::string::npos)
        {
            return checkParsedIPv6Address(ip);
        }

        // Catch-All for incorrect cases
        return false;
    }

    /**
     * @brief Wrapper method to check mixed bag of IP addresses and determine
     * validity of IPv4 or IPv6
     *
     * @param ip The IP address to check, type unknown
     */
    [[nodiscard]] bool
    checkValidityOfWhitelist(std::string const& ip) const
    {
        if (ip.find(".") != std::string::npos)
        {
            return isValidIPv4(ip);
        }

        else if (
            ip.find(":") != std::string::npos ||
            ip.find("::") != std::string::npos)
        {
            return isValidIPv6(ip);
        }

        else
        {
            return false;
        }
    }

    /**
     * @brief Wrapper method to check mixed bag of IPv4 and IPv6 subnets
     * and determine if an IP already fills a subnet
     *
     * @param ip The IP address to check, type unknown
     * @param subnetIP The subnet IP address to check, type unknown
     */
    [[nodiscard]] bool
    checkIfInSubnetMixed(std::string const& ip, std::string const& subnetIP)
        const
    {
        // Check that both addresses are IPv4
        if (ip.find(".") != std::string::npos &&
            subnetIP.find(".") != std::string::npos)
        {
            return isIPv4AddressInSubnet(ip, subnetIP);
        }

        // Check that both addresses are IPv6
        else if (
            (ip.find(":") != std::string::npos ||
             ip.find("::") != std::string::npos) &&
            (subnetIP.find(":") != std::string::npos ||
             subnetIP.find("::") != std::string::npos))
        {
            return isIPv6AddressInSubnet(ip, subnetIP);
        }

        else
        {
            return false;
        }
    }

    /**
     * @brief Constructs a new DOS guard.
     *
     * @param config Clio config
     * @param sweepHandler Sweep handler that implements the sweeping behaviour
     */
    BasicDOSGuard(clio::Config const& config, SweepHandler& sweepHandler)
        : whitelist_{getWhitelist(config)}
        , maxFetches_{config.valueOr("dos_guard.max_fetches", 1000000u)}
        , maxConnCount_{config.valueOr("dos_guard.max_connections", 20u)}
        , maxRequestCount_{config.valueOr("dos_guard.max_requests", 20u)}
    {
        sweepHandler.setup(this);
    }

    /**
     * @brief Check whether an ip address is in the whitelist or not
     *
     * @param ip The ip address to check (raw IP, NO CIDR)
     * @return true
     * @return falseconvertedIPsToStrings
     */
    [[nodiscard]] bool
    isWhiteListed(std::string const& ip) const noexcept
    {
        // CASE #1: Check if IP is CIDR. Treat as malformed input
        if (ip.find("/") != std::string::npos)
        {
            return false;
        }

        // CASE #2: Check if IP has malformed input.
        boost::system::error_code error_code;
        boost::asio::ip::address addr =
            boost::asio::ip::make_address(ip, error_code);

        // Neither a valid IPv4 nor a valid IPv6 address
        if (error_code)
        {
            // throw std::invalid_argument(error_code.message());
            return false;
        }

        // // CASE #3: O(1) | Check if IP has exact raw copy in whitelist
        // if (whitelist_.find(ip)!= whitelist_.end()) {
        //     return true;
        // }

        // CASE #4: If valid IP, O(N) run through vector
        for (auto const& ipInWhitelist : whitelist_)
        {
            // Raw match
            if (ip == ipInWhitelist)
            {
                return true;
            }

            // CIDR subnet match
            else if (checkIfInSubnetMixed(ip, ipInWhitelist) == true)
            {
                return true;
            }
        }

        // Only return false if there are no entries in the whitelist.
        return false;
    }

    /**
     * @brief Check whether an ip address is currently rate limited or not
     *
     * @param ip The ip address to check
     * @return true If not rate limited
     * @return false If rate limited and the request should not be processed
     */
    [[nodiscard]] bool
    isOk(std::string const& ip) const noexcept
    {
        if (whitelist_.contains(ip))
            return true;

        {
            std::scoped_lock lck(mtx_);
            if (ipState_.find(ip) != ipState_.end())
            {
                auto [transferedByte, requests] = ipState_.at(ip);
                if (transferedByte > maxFetches_ || requests > maxRequestCount_)
                {
                    log_.warn()
                        << "Dosguard:Client surpassed the rate limit. ip = "
                        << ip << " Transfered Byte:" << transferedByte
                        << " Requests:" << requests;
                    return false;
                }
            }
            auto it = ipConnCount_.find(ip);
            if (it != ipConnCount_.end())
            {
                if (it->second > maxConnCount_)
                {
                    log_.warn()
                        << "Dosguard:Client surpassed the rate limit. ip = "
                        << ip << " Concurrent connection:" << it->second;
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * @brief Increment connection count for the given ip address
     *
     * @param ip
     */
    void
    increment(std::string const& ip) noexcept
    {
        if (whitelist_.contains(ip))
            return;
        std::scoped_lock lck{mtx_};
        ipConnCount_[ip]++;
    }

    /**
     * @brief Decrement connection count for the given ip address
     *
     * @param ip
     */
    void
    decrement(std::string const& ip) noexcept
    {
        if (whitelist_.contains(ip))
            return;
        std::scoped_lock lck{mtx_};
        assert(ipConnCount_[ip] > 0);
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
        if (whitelist_.contains(ip))
            return true;

        {
            std::scoped_lock lck(mtx_);
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
        if (whitelist_.contains(ip))
            return true;

        {
            std::scoped_lock lck(mtx_);
            ipState_[ip].requestsCount++;
        }

        return isOk(ip);
    }

    /**
     * @brief Instantly clears all fetch counters added by @see add(std::string
     * const&, uint32_t)
     */
    void
    clear() noexcept override
    {
        std::scoped_lock lck(mtx_);
        ipState_.clear();
    }

private:
    [[nodiscard]] std::unordered_set<std::string> const
    getWhitelist(clio::Config const& config) const
    {
        // Data structures: string cast elements, complement, anwswer
        std::unordered_set<std::string> convertedIPsToStrings;
        std::unordered_set<std::string> subnetInstances;
        std::unordered_set<std::string> resultsSet;

        // Lambda function to cast to string
        auto const transform = [](auto const& elem) {
            return elem.template value<std::string>();
        };

        // Convert the RANGE of outputs to raw strings if not already
        auto preConversionList = config.arrayOr("dos_guard.whitelist", {});
        std::transform(
            std::begin(preConversionList),
            std::end(preConversionList),
            std::inserter(
                convertedIPsToStrings, std::end(convertedIPsToStrings)),
            transform);

        // O(N) | Parse the subnet instances in mixed bag BEFORE you compare all
        // the elements
        for (auto const& subnetIPIterator : convertedIPsToStrings)
        {
            // Push IPv4 or IPv6 subnets IPs if applicable
            if (subnetIPIterator.find("/") != std::string::npos &&
                checkValidityOfWhitelist(subnetIPIterator) == true)
            {
                // Insert to own data structure, make complement of set, insert
                // to result
                subnetInstances.insert(subnetIPIterator);
                resultsSet.insert(subnetIPIterator);
                convertedIPsToStrings.erase(subnetIPIterator);
            }
        }

        // O(N^2) | Automatic filtering of IPs that don't meet rules
        // NOTE: This is the COMPLEMENT SET of subnet
        for (auto const& candidateIPs : convertedIPsToStrings)
        {
            // Boolean values reset for every iteration
            bool inSubnet = false;

            // O(N) | Pass to check if the IP is in the subnet. If so, do NOT
            // add it bc subnet notation is sufficient anyways
            for (auto const& subnetInstance : subnetInstances)
            {
                if (checkIfInSubnetMixed(candidateIPs, subnetInstance) == true)
                {
                    inSubnet = true;
                }
            }

            // Only add IP iff not in a subnet AND it is a valid IPv4 or IPv6
            if (checkValidityOfWhitelist(candidateIPs) == true &&
                inSubnet == false)
            {
                resultsSet.insert(candidateIPs);
            }
        }

        return resultsSet;
    }
};

/**
 * @brief Sweep handler using a steady_timer and boost::asio::io_context.
 */
class IntervalSweepHandler
{
    std::chrono::milliseconds sweepInterval_;
    std::reference_wrapper<boost::asio::io_context> ctx_;
    BaseDOSGuard* dosGuard_ = nullptr;

    boost::asio::steady_timer timer_{ctx_.get()};

public:
    /**
     * @brief Construct a new interval-based sweep handler
     *
     * @param config Clio config
     * @param ctx The boost::asio::io_context
     */
    IntervalSweepHandler(
        clio::Config const& config,
        boost::asio::io_context& ctx)
        : sweepInterval_{std::max(
              1u,
              static_cast<uint32_t>(
                  config.valueOr("dos_guard.sweep_interval", 1.0) * 1000.0))}
        , ctx_{std::ref(ctx)}
    {
    }

    ~IntervalSweepHandler()
    {
        timer_.cancel();
    }

    /**
     * @brief This setup member function is called by @ref BasicDOSGuard during
     * its initialization.
     *
     * @param guard Pointer to the dos guard
     */
    void
    setup(BaseDOSGuard* guard)
    {
        assert(dosGuard_ == nullptr);
        dosGuard_ = guard;
        assert(dosGuard_ != nullptr);

        createTimer();
    }

private:
    void
    createTimer()
    {
        timer_.expires_after(sweepInterval_);
        timer_.async_wait([this](boost::system::error_code const& error) {
            if (error == boost::asio::error::operation_aborted)
                return;

            dosGuard_->clear();
            createTimer();
        });
    }
};

using DOSGuard = BasicDOSGuard<IntervalSweepHandler>;

}  // namespace clio
