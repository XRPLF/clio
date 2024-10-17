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

#include "util/newconfig/ArrayView.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ValueView.hpp"
#include "web/Resolver.hpp"
#include "web/dosguard/WhitelistHandlerInterface.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/asio/ip/network_v6.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <fmt/core.h>

#include <algorithm>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace web::dosguard {

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
    add(std::string_view net);

    /**
     * @brief Checks to see if ip address is whitelisted.
     *
     * @param ip IP address
     * @throws std::runtime::error when the network address is not valid
     * @return true if the given IP is whitelisted; false otherwise
     */
    bool
    isWhiteListed(std::string_view ip) const;

private:
    static bool
    isInV4Subnet(boost::asio::ip::address const& addr, boost::asio::ip::network_v4 const& subnet);

    static bool
    isInV6Subnet(boost::asio::ip::address const& addr, boost::asio::ip::network_v6 const& subnet);

    static bool
    isV4(std::string_view net);

    static bool
    isV6(std::string_view net);

    static bool
    isMask(std::string_view net);
};

/**
 * @brief A simple handler to add/check elements in a whitelist.
 */
class WhitelistHandler : public WhitelistHandlerInterface {
    Whitelist whitelist_;

public:
    /**
     * @brief Adds all whitelisted IPs and masks from the given config.
     *
     * @param config The Clio config to use
     * @param resolver The resolver to use for hostname resolution
     */
    template <SomeResolver HostnameResolverType = Resolver>
    WhitelistHandler(util::config::ClioConfigDefinition const& config, HostnameResolverType&& resolver = {})
    {
        std::unordered_set<std::string> const arr = getWhitelist(config, std::forward<HostnameResolverType>(resolver));
        for (auto const& net : arr)
            whitelist_.add(net);
    }

    /**
     * @brief Checks to see if the given IP is whitelisted
     *
     * @param ip The IP to check
     * @return true if the given IP is whitelisted; false otherwise
     */
    bool
    isWhiteListed(std::string_view ip) const override
    {
        return whitelist_.isWhiteListed(ip);
    }

private:
    template <SomeResolver HostnameResolverType>
    [[nodiscard]] static std::unordered_set<std::string>
    getWhitelist(util::config::ClioConfigDefinition const& config, HostnameResolverType&& resolver)
    {
        auto const whitelist = config.getArray("dos_guard.whitelist");
        std::unordered_set<std::string> hostnames{};
        // resolve hostnames to ips
        std::unordered_set<std::string> ips;

        if (whitelist.size() > 0) {
            for (auto it = whitelist.begin<util::config::ValueView>(); it != whitelist.end<util::config::ValueView>();
                 ++it)
                hostnames.insert((*it).asString());

            for (auto const& hostname : hostnames) {
                auto resolvedIps = resolver.resolve(hostname, "");
                for (auto& ip : resolvedIps) {
                    ips.insert(std::move(ip));
                }
            };
        }
        return ips;
    }
};

}  // namespace web::dosguard
