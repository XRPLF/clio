#include "web/Resolver.hpp"

#include "util/Assert.hpp"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace asio = boost::asio;

namespace web {

namespace {

// Check if the hostname is an IP address or a subnet.
bool
isAddress(std::string_view hostname)
{
    boost::system::error_code ec;
    asio::ip::make_address(hostname, ec);
    if (ec == boost::system::errc::success) {
        return true;
    }

    asio::ip::make_network_v4(hostname, ec);
    if (ec == boost::system::errc::success) {
        return true;
    }

    asio::ip::make_network_v6(hostname, ec);
    return ec == boost::system::errc::success;
}

std::string
toString(auto const& endpoint)
{
    std::stringstream ss;
    ss << endpoint;
    return ss.str();
}

}  // namespace

std::vector<std::string>
Resolver::resolve(std::string_view hostname, std::string_view service)
{
    ASSERT(not service.empty(), "Service is unspecified. Use `resolve(hostname)` instead.");

    if (isAddress(hostname))
        return {std::string(hostname) + ':' + std::string(service)};

    std::vector<std::string> endpoints;
    for (auto const& endpoint : doResolve(hostname, service))
        endpoints.push_back(toString(endpoint));

    return endpoints;
}

std::vector<std::string>
Resolver::resolve(std::string_view hostname)
{
    if (isAddress(hostname))
        return {std::string(hostname)};

    std::vector<std::string> endpoints;
    for (auto const& endpoint : doResolve(hostname, ""))
        endpoints.push_back(endpoint.address().to_string());

    return endpoints;
}

std::vector<boost::asio::ip::tcp::endpoint>
Resolver::doResolve(std::string_view hostname, std::string_view service)
{
    std::vector<boost::asio::ip::tcp::endpoint> endpoints;
    for (auto&& endpoint : resolver_.resolve(hostname, service))
        endpoints.push_back(std::move(endpoint));

    return endpoints;
}

}  // namespace web
