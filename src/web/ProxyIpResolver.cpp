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

#include "web/ProxyIpResolver.hpp"

#include "util/JsonUtils.hpp"
#include "util/Shasum.hpp"
#include "util/config/ArrayView.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ValueView.hpp"

#include <boost/beast/http/field.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace web {

ProxyIpResolver::ProxyIpResolver(
    std::unordered_set<std::string> proxyIps,
    std::unordered_set<std::string> proxyTokens
)
    : proxyIps_(std::move(proxyIps))
{
    proxyTokens_.reserve(proxyTokens.size());
    for (auto const& t : proxyTokens) {
        proxyTokens_.insert(util::sha256sum(t));
    }
}

ProxyIpResolver
ProxyIpResolver::fromConfig(util::config::ClioConfigDefinition const& config)
{
    using util::config::ValueView;

    std::unordered_set<std::string> ips;
    auto const ipsFromConfig = config.getArray("server.proxy.ips");
    for (auto it = ipsFromConfig.begin<ValueView>(); it != ipsFromConfig.end<ValueView>(); ++it) {
        ips.insert((*it).asString());
    }

    std::unordered_set<std::string> tokens;
    auto const tokensFromConfig = config.getArray("server.proxy.tokens");
    for (auto it = tokensFromConfig.begin<ValueView>(); it != tokensFromConfig.end<ValueView>();
         ++it) {
        tokens.insert((*it).asString());
    }

    return ProxyIpResolver{std::move(ips), std::move(tokens)};
}

std::string
ProxyIpResolver::resolveClientIp(std::string const& connectionIp, HttpHeaders const& headers) const
{
    if (proxyIps_.contains(connectionIp)) {
        return extractClientIp(headers).value_or(connectionIp);
    }

    if (auto it = headers.find(kPROXY_TOKEN_HEADER); it != headers.end()) {
        auto const tokenHash = util::sha256sum(it->value());
        if (proxyTokens_.contains(tokenHash)) {
            return extractClientIp(headers).value_or(connectionIp);
        }
    }
    return connectionIp;
}

std::optional<std::string>
ProxyIpResolver::extractClientIp(HttpHeaders const& headers)
{
    auto const it = headers.find(boost::beast::http::field::forwarded);
    if (it == headers.end()) {
        return std::nullopt;
    }

    // Forwarded header is case insensitive:
    // https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Forwarded#using_the_forwarded_header
    auto const headerValue = util::toLower(it->value());

    static constexpr std::string_view kFOR_PREFIX = "for=";
    auto const startPos = headerValue.find(kFOR_PREFIX);
    if (startPos == std::string::npos) {
        return std::nullopt;
    }
    auto value = it->value().substr(startPos + kFOR_PREFIX.size());

    static constexpr char kDELIMITER = ';';
    auto const endPos = value.find(kDELIMITER);
    auto const ip = value.substr(0, endPos);

    static constexpr auto kMIN_IP_LENGTH = 7;  // minimum 3 dots + 4 digits
    if (ip.size() < kMIN_IP_LENGTH) {
        return std::nullopt;
    }

    if (ip.starts_with('"')) {
        return ip.substr(1, ip.size() - 2);
    }
    return ip;
}

}  // namespace web
