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

#include "web/Resolver.h"

#include <boost/asio/ip/address.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace asio = boost::asio;

namespace web {

namespace {

// Check if the hostname is an IP address or a subnet.
bool
isAddress(std::string_view hostname)
{
    boost::system::error_code ec;
    asio::ip::make_address(hostname.data(), ec);
    if (ec == boost::system::errc::success) {
        return true;
    }

    asio::ip::make_network_v4(hostname.data(), ec);
    if (ec == boost::system::errc::success) {
        return true;
    }

    asio::ip::make_network_v6(hostname.data(), ec);
    return ec == boost::system::errc::success;
}

}  // namespace

std::vector<std::string>
Resolver::resolve(std::string_view hostname, std::string_view service)
{
    if (isAddress(hostname)) {
        return {std::string(hostname)};
    }

    std::vector<std::string> endpoints;
    for (auto const& endpoint : resolver_.resolve(hostname, service)) {
        endpoints.push_back(endpoint.endpoint().address().to_string());
    }
    return endpoints;
}

}  // namespace web
