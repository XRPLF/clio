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

#include "web/dosguard/WhitelistHandler.hpp"

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

namespace web::dosguard {

void
Whitelist::add(std::string_view net)
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

bool
Whitelist::isWhiteListed(std::string_view ip) const
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

bool
Whitelist::isInV4Subnet(boost::asio::ip::address const& addr, boost::asio::ip::network_v4 const& subnet)
{
    auto const range = subnet.hosts();
    return range.find(addr.to_v4()) != range.end();
}

bool
Whitelist::isInV6Subnet(boost::asio::ip::address const& addr, boost::asio::ip::network_v6 const& subnet)
{
    auto const range = subnet.hosts();
    return range.find(addr.to_v6()) != range.end();
}

bool
Whitelist::isV4(std::string_view net)
{
    static std::regex const ipv4CidrRegex(R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/\d{1,2}$)");
    return std::regex_match(std::string(net), ipv4CidrRegex);
}

bool
Whitelist::isV6(std::string_view net)
{
    static std::regex const ipv6CidrRegex(R"(^([0-9A-Fa-f]{1,4}:){7}[0-9A-Fa-f]{1,4}/\d{1,3}$)");
    return std::regex_match(std::string(net), ipv6CidrRegex);
}

bool
Whitelist::isMask(std::string_view net)
{
    return net.find('/') != std::string_view::npos;
}

}  // namespace web::dosguard
