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

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <concepts>
#include <string>
#include <string_view>
#include <vector>

namespace web {

template <typename T>
concept SomeResolver = requires(T t) {
    std::is_default_constructible_v<T>;
    {
        t.resolve(std::string_view{}, std::string_view{})
    } -> std::same_as<std::vector<std::string>>;
};

/**
 * @brief Simple hostnames to IP addresses resolver.
 */
class Resolver {
public:
    /**
     * @brief Resolve hostname to IP addresses.
     *
     * @throw This method throws an exception when the hostname cannot be resolved.
     *
     * @param hostname Hostname to resolve
     * @param service Service to resolve (could be empty or port number or http)
     * @return IP addresses of the hostname
     */
    std::vector<std::string>
    resolve(std::string_view hostname, std::string_view service = "");

private:
    boost::asio::io_context ioContext_;
    boost::asio::ip::tcp::resolver resolver_{ioContext_};
};

}  // namespace web
