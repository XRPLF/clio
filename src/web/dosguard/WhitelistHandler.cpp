#include "web/dosguard/WhitelistHandler.hpp"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/asio/ip/network_v6.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <functional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace web::dosguard {

WhitelistHandler::WhitelistHandler(Whitelist whitelist) : whitelist_(std::move(whitelist))
{
}

std::expected<void, std::string>
Whitelist::add(std::string_view net)
{
    using namespace boost::asio;

    if (not isMask(net)) {
        boost::system::error_code ec;
        auto const ip = ip::make_address(net, ec);
        if (ec.failed())
            return std::unexpected{fmt::format("Malformed whitelist ip address: {}. ", net)};
        ips_.push_back(ip);
        return {};
    }

    if (isV4(net)) {
        boost::system::error_code ec;
        auto const net4 = ip::make_network_v4(net, ec);
        if (ec.failed())
            return std::unexpected{fmt::format("Malformed network: {}. ", net)};
        subnetsV4_.push_back(net4);
    } else if (isV6(net)) {
        boost::system::error_code ec;
        auto const net6 = ip::make_network_v6(net, ec);
        if (ec.failed())
            return std::unexpected{fmt::format("Malformed network: {}. ", net)};
        subnetsV6_.push_back(net6);
    } else {
        return std::unexpected{fmt::format("Malformed network: {}. ", net)};
    }
    return {};
}

bool
Whitelist::isWhiteListed(std::string_view ip) const
{
    using namespace boost::asio;

    boost::system::error_code ec;
    auto const addr = ip::make_address(ip, ec);
    if (ec.failed())
        return false;

    if (std::ranges::find(ips_, addr) != std::end(ips_))
        return true;

    if (addr.is_v4()) {
        return std::ranges::find_if(subnetsV4_, std::bind_front(&isInV4Subnet, std::cref(addr))) !=
            std::end(subnetsV4_);
    }

    if (addr.is_v6()) {
        return std::ranges::find_if(subnetsV6_, std::bind_front(&isInV6Subnet, std::cref(addr))) !=
            std::end(subnetsV6_);
    }

    return false;
}

bool
Whitelist::isInV4Subnet(
    boost::asio::ip::address const& addr,
    boost::asio::ip::network_v4 const& subnet
)
{
    auto const range = subnet.hosts();
    return range.find(addr.to_v4()) != range.end();
}

bool
Whitelist::isInV6Subnet(
    boost::asio::ip::address const& addr,
    boost::asio::ip::network_v6 const& subnet
)
{
    auto const range = subnet.hosts();
    return range.find(addr.to_v6()) != range.end();
}

bool
Whitelist::isV4(std::string_view net)
{
    static std::regex const kIPV4_CIDR_REGEX(R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/\d{1,2}$)");
    return std::regex_match(std::string(net), kIPV4_CIDR_REGEX);
}

bool
Whitelist::isV6(std::string_view net)
{
    static std::regex const kIPV6_CIDR_REGEX(R"(^([0-9A-Fa-f]{1,4}:){7}[0-9A-Fa-f]{1,4}/\d{1,3}$)");
    return std::regex_match(std::string(net), kIPV6_CIDR_REGEX);
}

bool
Whitelist::isMask(std::string_view net)
{
    return net.contains('/');
}

}  // namespace web::dosguard
