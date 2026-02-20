//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "util/config/ConfigDefinition.hpp"
#include "util/config/ValueView.hpp"

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <xrpl/basics/base_uint.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace web {

/**
 * @brief Resolves the client's IP address, considering proxy servers.
 *
 * This class is designed to determine the original IP address of a client when the connection
 * is forwarded through a proxy server. It uses a configurable list of trusted proxy IPs
 * and proxy tokens to decide whether to trust the `Forwarded` HTTP header.
 */
class ProxyIpResolver {
    std::unordered_set<std::string> proxyIps_;
    // ripple::uint256 doesn't have hash implementation
    std::unordered_set<ripple::uint256, ripple::uint256::hasher> proxyTokens_;

public:
    /**
     * @brief Constructs a ProxyIpResolver.
     *
     * @param proxyIps A set of trusted proxy IP addresses.
     * @param proxyTokens A set of trusted proxy tokens. The tokens will be hashed with SHA-256.
     */
    ProxyIpResolver(
        std::unordered_set<std::string> proxyIps,
        std::unordered_set<std::string> proxyTokens
    );

    /**
     * @brief Creates a ProxyIpResolver from a configuration.
     *
     * The configuration should contain `server.proxy.ips` and `server.proxy.tokens` arrays.
     *
     * @param config The Clio configuration.
     * @return A new ProxyIpResolver instance.
     */
    static ProxyIpResolver
    fromConfig(util::config::ClioConfigDefinition const& config);

    using HttpHeaders = boost::beast::http::request<boost::beast::http::string_body>::header_type;

    static constexpr std::string_view kPROXY_TOKEN_HEADER = "X-Proxy-Token";

    /**
     * @brief Resolves the client's IP address from the connection IP and HTTP headers.
     *
     * If the connection IP is in the trusted proxy list, or if a valid proxy token is provided in
     * the headers, this method will attempt to extract the client's IP from the `Forwarded` header.
     * Otherwise, it returns the connection IP.
     *
     * @param connectionIp The IP address of the direct connection.
     * @param headers The HTTP request headers.
     * @return The resolved client IP address as a string.
     */
    std::string
    resolveClientIp(std::string const& connectionIp, HttpHeaders const& headers) const;

private:
    /**
     * @brief Extracts the client IP from the `Forwarded` HTTP header.
     *
     * The `Forwarded` header is expected to be in the format specified by RFC 7239.
     * This function looks for the `for` parameter.
     *
     * @param headers The HTTP request headers.
     * @return The client IP address as a string if found, otherwise std::nullopt.
     */
    static std::optional<std::string>
    extractClientIp(HttpHeaders const& headers);
};

}  // namespace web
