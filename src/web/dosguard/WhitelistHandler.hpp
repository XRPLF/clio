#pragma once

#include "util/config/ArrayView.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ValueView.hpp"
#include "web/Resolver.hpp"
#include "web/dosguard/WhitelistHandlerInterface.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/asio/ip/network_v6.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
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
     * @return void on success, or an error string if the address is not valid
     */
    std::expected<void, std::string>
    add(std::string_view net);

    /**
     * @brief Checks to see if ip address is whitelisted.
     *
     * @param ip IP address
     * @return true if the given IP is whitelisted; false otherwise
     */
    [[nodiscard]] bool
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
     * @brief Constructs a WhitelistHandler from an already-built Whitelist.
     *
     * @param whitelist The whitelist to use
     */
    explicit WhitelistHandler(Whitelist whitelist);

    /**
     * @brief Creates a WhitelistHandler by loading all whitelisted IPs and masks from config.
     *
     * @param config The Clio config to use
     * @param resolver The resolver to use for hostname resolution
     * @return The WhitelistHandler on success, or an error string if any whitelist entry is invalid
     */
    template <SomeResolver HostnameResolverType = Resolver>
    static std::expected<WhitelistHandler, std::string>
    create(util::config::ClioConfigDefinition const& config, HostnameResolverType&& resolver = {})
    {
        std::unordered_set<std::string> const arr =
            getWhitelist(config, std::forward<HostnameResolverType>(resolver));
        Whitelist whitelist;
        std::optional<std::string> errors;
        for (auto const& net : arr) {
            if (auto result = whitelist.add(net); !result.has_value()) {
                if (!errors.has_value())
                    errors.emplace();
                errors->append(std::move(result).error());
            }
        }
        if (errors.has_value()) {
            return std::unexpected{std::move(errors).value()};
        }
        return WhitelistHandler(std::move(whitelist));
    }

    /**
     * @brief Checks to see if the given IP is whitelisted
     *
     * @param ip The IP to check
     * @return true if the given IP is whitelisted; false otherwise
     */
    [[nodiscard]] bool
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

        for (auto it = whitelist.begin<util::config::ValueView>();
             it != whitelist.end<util::config::ValueView>();
             ++it)
            hostnames.insert((*it).asString());

        for (auto const& hostname : hostnames) {
            auto resolvedIps = resolver.resolve(hostname);
            for (auto& ip : resolvedIps) {
                ips.insert(std::move(ip));
            }
        };

        return ips;
    }
};

}  // namespace web::dosguard
