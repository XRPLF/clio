#include "util/NameGenerator.hpp"
#include "util/config/Array.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigFileJson.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "web/ProxyIpResolver.hpp"

#include <boost/beast/http/field.hpp>
#include <boost/json/parse.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace http = boost::beast::http;

using namespace web;

struct ProxyIpResolverTestParams {
    std::string testName;
    std::unordered_set<std::string> proxyIps;
    std::unordered_set<std::string> proxyTokens;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string connectionIp;
    std::optional<std::string> expectedIp;
};

class ProxyIpResolverTest : public ::testing::TestWithParam<ProxyIpResolverTestParams> {};

TEST_F(ProxyIpResolverTest, FromConfig)
{
    using namespace util::config;
    ClioConfigDefinition config{{
        {"server.proxy.ips.[]", Array{ConfigValue{ConfigType::String}}},
        {"server.proxy.tokens.[]", Array{ConfigValue{ConfigType::String}}},
    }};
    auto const proxyIp = "1.2.3.4";
    auto const clientIp = "5.6.7.8";
    auto const proxyToken = "some_proxy_token";

    auto const configStr = fmt::format(
        R"({{
        "server": {{
            "proxy": {{
                "ips": ["{}"],
                "tokens": ["{}"]
            }}
        }}
    }})",
        proxyIp,
        proxyToken
    );

    auto const err = config.parse(ConfigFileJson{boost::json::parse(configStr).as_object()});
    ASSERT_FALSE(err.has_value());

    auto const proxyIpResolver = ProxyIpResolver::fromConfig(config);
    ProxyIpResolver::HttpHeaders headers;

    EXPECT_EQ(proxyIpResolver.resolveClientIp(clientIp, headers), std::nullopt);
    EXPECT_EQ(proxyIpResolver.resolveClientIp(proxyIp, headers), std::nullopt);

    headers.set(boost::beast::http::field::forwarded, fmt::format("for={}", clientIp));
    EXPECT_EQ(proxyIpResolver.resolveClientIp(clientIp, headers), std::nullopt);
    EXPECT_EQ(proxyIpResolver.resolveClientIp(proxyIp, headers), clientIp);

    headers.set(ProxyIpResolver::kPROXY_TOKEN_HEADER, proxyToken);
    EXPECT_EQ(proxyIpResolver.resolveClientIp(clientIp, headers), clientIp);
    EXPECT_EQ(proxyIpResolver.resolveClientIp(proxyIp, headers), clientIp);
    EXPECT_EQ(proxyIpResolver.resolveClientIp("127.0.0.1", headers), clientIp);
}

TEST_P(ProxyIpResolverTest, ResolveClientIp)
{
    auto const& params = GetParam();
    ProxyIpResolver const resolver(params.proxyIps, params.proxyTokens);
    ProxyIpResolver::HttpHeaders headers;
    for (auto const& [key, value] : params.headers) {
        headers.set(key, value);
    }
    EXPECT_EQ(resolver.resolveClientIp(params.connectionIp, headers), params.expectedIp);
}

INSTANTIATE_TEST_SUITE_P(
    ProxyIpResolverTests,
    ProxyIpResolverTest,
    ::testing::Values(
        ProxyIpResolverTestParams{
            .testName = "NoProxy",
            .proxyIps = {},
            .proxyTokens = {},
            .headers = {},
            .connectionIp = "1.2.3.4",
            .expectedIp = std::nullopt
        },
        ProxyIpResolverTestParams{
            .testName = "TrustedProxyIpWithForwardedHeader",
            .proxyIps = {"5.6.7.8"},
            .proxyTokens = {},
            .headers = {{std::string(http::to_string(http::field::forwarded)), "for=1.2.3.4"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = "1.2.3.4"
        },
        ProxyIpResolverTestParams{
            .testName = "TrustedProxyIpWithoutForwardedHeader",
            .proxyIps = {"5.6.7.8"},
            .proxyTokens = {},
            .headers = {},
            .connectionIp = "5.6.7.8",
            .expectedIp = std::nullopt
        },
        ProxyIpResolverTestParams{
            .testName = "UntrustedProxyIpWithForwardedHeader",
            .proxyIps = {},
            .proxyTokens = {},
            .headers = {{std::string(http::to_string(http::field::forwarded)), "for=1.2.3.4"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = std::nullopt
        },
        ProxyIpResolverTestParams{
            .testName = "TrustedProxyTokenWithForwardedHeader",
            .proxyIps = {},
            .proxyTokens = {"test_token"},
            .headers =
                {{std::string(ProxyIpResolver::kPROXY_TOKEN_HEADER), "test_token"},
                 {std::string(http::to_string(http::field::forwarded)), "for=1.2.3.4"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = "1.2.3.4"
        },
        ProxyIpResolverTestParams{
            .testName = "TrustedProxyTokenWithoutForwardedHeader",
            .proxyIps = {},
            .proxyTokens = {"test_token"},
            .headers = {{std::string(ProxyIpResolver::kPROXY_TOKEN_HEADER), "test_token"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = std::nullopt
        },
        ProxyIpResolverTestParams{
            .testName = "UntrustedProxyTokenWithForwardedHeader",
            .proxyIps = {},
            .proxyTokens = {},
            .headers =
                {{std::string(ProxyIpResolver::kPROXY_TOKEN_HEADER), "test_token"},
                 {std::string(http::to_string(http::field::forwarded)), "for=1.2.3.4"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = std::nullopt
        },
        ProxyIpResolverTestParams{
            .testName = "ForwardedHeaderWithAdditionalFields",
            .proxyIps = {"5.6.7.8"},
            .proxyTokens = {},
            .headers =
                {{std::string(http::to_string(http::field::forwarded)),
                  "by=203.0.113.43; for=1.2.3.4; host=example.com; proto=https"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = "1.2.3.4"
        },
        ProxyIpResolverTestParams{
            .testName = "ForwardedHeaderWithDifferentCase",
            .proxyIps = {"5.6.7.8"},
            .proxyTokens = {},
            .headers = {{std::string(http::to_string(http::field::forwarded)), "For=1.2.3.4"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = "1.2.3.4"
        },
        ProxyIpResolverTestParams{
            .testName = "ForwardedHeaderWithoutFor",
            .proxyIps = {"5.6.7.8"},
            .proxyTokens = {},
            .headers = {{std::string(http::to_string(http::field::forwarded)), "by=1.2.3.4"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = std::nullopt
        },
        ProxyIpResolverTestParams{
            .testName = "ForwardedHeaderWithIpInQuotes",
            .proxyIps = {"5.6.7.8"},
            .proxyTokens = {},
            .headers = {{std::string(http::to_string(http::field::forwarded)), "for=\"1.2.3.4\""}},
            .connectionIp = "5.6.7.8",
            .expectedIp = "1.2.3.4"
        },
        ProxyIpResolverTestParams{
            .testName = "ForwardedHeaderIsIncorrect",
            .proxyIps = {"5.6.7.8"},
            .proxyTokens = {},
            .headers =
                {{std::string(http::to_string(http::field::forwarded)), "for=\";some_other_text"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = std::nullopt
        },
        ProxyIpResolverTestParams{
            .testName = "ForwardedHeaderWithMultipleForValues",
            .proxyIps = {"5.6.7.8"},
            .proxyTokens = {},
            .headers =
                {{std::string(http::to_string(http::field::forwarded)),
                  "for=1.2.3.4, for=9.10.11.12"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = "9.10.11.12"
        },
        ProxyIpResolverTestParams{
            .testName = "ForwardedHeaderWithMultipleForValuesAndSectionDelimiters",
            .proxyIps = {"5.6.7.8"},
            .proxyTokens = {},
            .headers =
                {{std::string(http::to_string(http::field::forwarded)),
                  "for=1.2.3.4; proto=http, for=9.10.11.12; proto=https"}},
            .connectionIp = "5.6.7.8",
            .expectedIp = "9.10.11.12"
        }
    ),
    tests::util::kNAME_GENERATOR
);
