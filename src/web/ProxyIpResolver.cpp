#include "web/ProxyIpResolver.hpp"

#include "util/JsonUtils.hpp"
#include "util/Shasum.hpp"
#include "util/config/ArrayView.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ValueView.hpp"

#include <boost/beast/http/field.hpp>

#include <algorithm>
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

std::optional<std::string>
ProxyIpResolver::resolveClientIp(std::string const& connectionIp, HttpHeaders const& headers) const
{
    if (proxyIps_.contains(connectionIp)) {
        return extractClientIp(headers);
    }

    if (auto it = headers.find(kProxyTokenHeader); it != headers.end()) {
        auto const tokenHash = util::sha256sum(it->value());
        if (proxyTokens_.contains(tokenHash)) {
            return extractClientIp(headers);
        }
    }
    return std::nullopt;
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

    static constexpr std::string_view kForPrefix = "for=";
    auto const startPos = headerValue.rfind(kForPrefix);
    if (startPos == std::string::npos) {
        return std::nullopt;
    }
    auto value = it->value().substr(startPos + kForPrefix.size());

    static constexpr char kSectionDelimiter = ';';
    static constexpr char kChainDelimiter = ',';
    auto const sectionEnd = value.find(kSectionDelimiter);
    auto const chainEnd = value.find(kChainDelimiter);
    auto const endPos = std::min(sectionEnd, chainEnd);
    auto const ip = value.substr(0, endPos);

    static constexpr auto kMinIpLength = 7;  // minimum 3 dots + 4 digits
    if (ip.size() < kMinIpLength) {
        return std::nullopt;
    }

    if (ip.starts_with('"')) {
        return ip.substr(1, ip.size() - 2);
    }
    return ip;
}

}  // namespace web
