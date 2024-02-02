//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "util/config/Config.hpp"
#include "web/Resolver.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/asio/ip/network_v6.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <functional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace web {

/**
 * @brief A whitelist to remove rate limits of certain IP addresses.
 */
class Whitelist {
    std::vector<boost::asio::ip::network_v4> subnetsV4_;
    std::vector<boost::asio::ip::network_v6> subnetsV6_;
    std::vector<boost::asio::ip::address> ips_;

public:
    /**
     * @brief Add network address to whitelist.
     *
     * @param net Network part of the ip address
     * @throws std::runtime::error when the network address is not valid
     */
    void
    add(std::string_view net)
    {
        using namespace boost::asio;

        if (not isMask(net)) {
            ips_.push_back(ip::make_address(net));
            return;
        }

        if (isV4(net)) {
            subnetsV4_.push_back(ip::make_network_v4(net));
        } else if (isV6(net)) {
            subnetsV6_.push_back(ip::make_network_v6(net));
        } else {
            throw std::runtime_error(fmt::format("malformed network: {}", net.data()));
        }
    }

    /**
     * @brief Checks to see if ip address is whitelisted.
     *
     * @param ip IP address
     * @throws std::runtime::error when the network address is not valid
     */
    bool
    isWhiteListed(std::string_view ip) const
    {
        using namespace boost::asio;

        auto const addr = ip::make_address(ip);
        if (std::find(std::begin(ips_), std::end(ips_), addr) != std::end(ips_))
            return true;

        if (addr.is_v4()) {
            return std::find_if(
                       std::begin(subnetsV4_), std::end(subnetsV4_), std::bind_front(&isInV4Subnet, std::cref(addr))
                   ) != std::end(subnetsV4_);
        }

        if (addr.is_v6()) {
            return std::find_if(
                       std::begin(subnetsV6_), std::end(subnetsV6_), std::bind_front(&isInV6Subnet, std::cref(addr))
                   ) != std::end(subnetsV6_);
        }

        return false;
    }

private:
    static bool
    isInV4Subnet(boost::asio::ip::address const& addr, boost::asio::ip::network_v4 const& subnet)
    {
        auto const range = subnet.hosts();
        return range.find(addr.to_v4()) != range.end();
    }

    static bool
    isInV6Subnet(boost::asio::ip::address const& addr, boost::asio::ip::network_v6 const& subnet)
    {
        auto const range = subnet.hosts();
        return range.find(addr.to_v6()) != range.end();
    }

    static bool
    isV4(std::string_view net)
    {
        static std::regex const ipv4CidrRegex(R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/\d{1,2}$)");
        return std::regex_match(std::string(net), ipv4CidrRegex);
    }

    static bool
    isV6(std::string_view net)
    {
        static std::regex const ipv6CidrRegex(R"(^([0-9A-Fa-f]{1,4}:){7}[0-9A-Fa-f]{1,4}/\d{1,3}$)");
        return std::regex_match(std::string(net), ipv6CidrRegex);
    }

    static bool
    isMask(std::string_view net)
    {
        return net.find('/') != std::string_view::npos;
    }
};

/**
 * @brief A simple handler to add/check elements in a whitelist.
 */
class WhitelistHandler {
    Whitelist whitelist_;

public:
    /**
     * @brief Adds all whitelisted IPs and masks from the given config.
     *
     * @param config The Clio config to use
     */
    template <SomeResolver HostnameResolverType = Resolver>
    WhitelistHandler(util::Config const& config, HostnameResolverType&& resolver = {})
    {
        std::unordered_set<std::string> const arr = getWhitelist(config, std::forward<HostnameResolverType>(resolver));
        for (auto const& net : arr)
            whitelist_.add(net);
    }

    /**
     * @return true if the given IP is whitelisted; false otherwise
     */
    bool
    isWhiteListed(std::string_view ip) const
    {
        return whitelist_.isWhiteListed(ip);
    }

private:
    template <SomeResolver HostnameResolverType>
    [[nodiscard]] static std::unordered_set<std::string>
    getWhitelist(util::Config const& config, HostnameResolverType&& resolver)
    {
        auto whitelist = config.arrayOr("dos_guard.whitelist", {});
        auto const transform = [](auto const& elem) { return elem.template value<std::string>(); };

        std::unordered_set<std::string> const hostnames{
            boost::transform_iterator(std::begin(whitelist), transform),
            boost::transform_iterator(std::end(whitelist), transform)
        };

        // resolve hostnames to ips
        std::unordered_set<std::string> ips;
        for (auto const& hostname : hostnames) {
            auto resolvedIps = resolver.resolve(hostname, "");
            for (auto& ip : resolvedIps) {
                ips.insert(std::move(ip));
            }
        };
        return ips;
    }
};

}  // namespace web
