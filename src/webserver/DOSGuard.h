//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <config/Config.h>

#include <boost/asio.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <memory>
#include <optional>
#include <random>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace boost::asio;

namespace clio {

class BaseDOSGuard
{
public:
    virtual ~BaseDOSGuard() = default;

    virtual void
    clear() noexcept = 0;
};

class Whitelist
{
    std::vector<ip::network_v4> subnetsV4_;
    std::vector<ip::network_v6> subnetsV6_;
    std::vector<ip::address> ips_;

    static bool
    isInV4Subnet(ip::address const& addr, ip::network_v4 const& subnet)
    {
        auto const range = subnet.hosts();
        return range.find(addr.to_v4()) != range.end();
    }

    static bool
    isInV6Subnet(ip::address const& addr, ip::network_v6 const& subnet)
    {
        auto const range = subnet.hosts();
        return range.find(addr.to_v6()) != range.end();
    }

public:
    // could throw, handle with care
    void
    add(std::string_view net)
    {
        if (not isMask(net))
        {
            ips_.push_back(ip::make_address(net));
            return;
        }

        if (isV4(net))
            subnetsV4_.push_back(ip::make_network_v4(net));
        else if (isV6(net))
            subnetsV6_.push_back(ip::make_network_v6(net));
        else
            throw std::runtime_error(std::string{"malformed network: "} + net.data());
    }

    // could throw, handle with care
    bool
    isWhiteListed(std::string_view ip) const
    {
        auto const addr = ip::make_address(ip);
        if (std::find(std::begin(ips_), std::end(ips_), addr) != std::end(ips_))
            return true;

        if (addr.is_v4())
            return std::find_if(
                       std::begin(subnetsV4_), std::end(subnetsV4_), std::bind_front(&isInV4Subnet, std::cref(addr))) !=
                std::end(subnetsV4_);

        if (addr.is_v6())
            return std::find_if(
                       std::begin(subnetsV6_), std::end(subnetsV6_), std::bind_front(&isInV6Subnet, std::cref(addr))) !=
                std::end(subnetsV6_);

        return false;
    }

private:
    bool
    isV4(std::string_view net) const
    {
        static const std::regex ipv4CidrRegex(R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/\d{1,2}$)");
        return std::regex_match(std::string(net), ipv4CidrRegex);
    }
    bool
    isV6(std::string_view net) const
    {
        static const std::regex ipv6CidrRegex(R"(^([0-9A-Fa-f]{1,4}:){7}[0-9A-Fa-f]{1,4}/\d{1,3}$)");
        return std::regex_match(std::string(net), ipv6CidrRegex);
    }
    bool
    isMask(std::string_view net) const
    {
        return net.find('/') != std::string_view::npos;
    }
};

class WhitelistHandler
{
    Whitelist whitelist_;

public:
    WhitelistHandler(std::unordered_set<std::string> const& arr)
    {
        for (auto const& net : arr)
            whitelist_.add(net);
    }

    bool
    isWhiteListed(std::string_view ip) const
    {
        return whitelist_.isWhiteListed(ip);
    }
};

/**
 * @brief A simple denial of service guard used for rate limiting.
 *
 * @tparam SweepHandler Type of the sweep handler
 */
template <typename WhitelistHandlerType, typename SweepHandler>
class BasicDOSGuard : public BaseDOSGuard
{
    // Accumulated state per IP, state will be reset accordingly
    struct ClientState
    {
        // accumulated transfered byte
        std::uint32_t transferedByte = 0;
        // accumulated served requests count
        std::uint32_t requestsCount = 0;
    };

    mutable std::mutex mtx_;
    // accumulated states map
    std::unordered_map<std::string, ClientState> ipState_;
    std::unordered_map<std::string, std::uint32_t> ipConnCount_;
    WhitelistHandlerType whitelistHandler_;

    std::uint32_t const maxFetches_;
    std::uint32_t const maxConnCount_;
    std::uint32_t const maxRequestCount_;
    clio::Logger log_{"RPC"};

public:
    /**
     * @brief Constructs a new DOS guard.
     *
     * @param config Clio config
     * @param sweepHandler Sweep handler that implements the sweeping behaviour
     */
    BasicDOSGuard(clio::Config const& config, SweepHandler& sweepHandler)
        : whitelistHandler_{getWhitelist(config)}
        , maxFetches_{config.valueOr("dos_guard.max_fetches", 1000000u)}
        , maxConnCount_{config.valueOr("dos_guard.max_connections", 20u)}
        , maxRequestCount_{config.valueOr("dos_guard.max_requests", 20u)}
    {
        sweepHandler.setup(this);
    }

    /**
     * @brief Check whether an ip address is in the whitelist or not
     *
     * @param ip The ip address to check
     * @return true
     * @return false
     */
    [[nodiscard]] bool
    isWhiteListed(std::string_view const& ip) const noexcept
    {
        return whitelistHandler_.isWhiteListed(ip);
    }

    /**
     * @brief Check whether an ip address is currently rate limited or not
     *
     * @param ip The ip address to check
     * @return true If not rate limited
     * @return false If rate limited and the request should not be processed
     */
    [[nodiscard]] bool
    isOk(std::string const& ip) const noexcept
    {
        if (whitelistHandler_.isWhiteListed(ip))
            return true;

        {
            std::scoped_lock lck(mtx_);
            if (ipState_.find(ip) != ipState_.end())
            {
                auto [transferedByte, requests] = ipState_.at(ip);
                if (transferedByte > maxFetches_ || requests > maxRequestCount_)
                {
                    log_.warn() << "Dosguard:Client surpassed the rate limit. ip = " << ip
                                << " Transfered Byte:" << transferedByte << " Requests:" << requests;
                    return false;
                }
            }
            auto it = ipConnCount_.find(ip);
            if (it != ipConnCount_.end())
            {
                if (it->second > maxConnCount_)
                {
                    log_.warn() << "Dosguard:Client surpassed the rate limit. ip = " << ip
                                << " Concurrent connection:" << it->second;
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * @brief Increment connection count for the given ip address
     *
     * @param ip
     */
    void
    increment(std::string const& ip) noexcept
    {
        if (whitelistHandler_.isWhiteListed(ip))
            return;
        std::scoped_lock lck{mtx_};
        ipConnCount_[ip]++;
    }

    /**
     * @brief Decrement connection count for the given ip address
     *
     * @param ip
     */
    void
    decrement(std::string const& ip) noexcept
    {
        if (whitelistHandler_.isWhiteListed(ip))
            return;
        std::scoped_lock lck{mtx_};
        assert(ipConnCount_[ip] > 0);
        ipConnCount_[ip]--;
        if (ipConnCount_[ip] == 0)
            ipConnCount_.erase(ip);
    }

    /**
     * @brief Adds numObjects of usage for the given ip address.
     *
     * If the total sums up to a value equal or larger than maxFetches_
     * the operation is no longer allowed and false is returned; true is
     * returned otherwise.
     *
     * @param ip
     * @param numObjects
     * @return true
     * @return false
     */
    [[maybe_unused]] bool
    add(std::string const& ip, uint32_t numObjects) noexcept
    {
        if (whitelistHandler_.isWhiteListed(ip))
            return true;

        {
            std::scoped_lock lck(mtx_);
            ipState_[ip].transferedByte += numObjects;
        }

        return isOk(ip);
    }

    /**
     * @brief Adds one request for the given ip address.
     *
     * If the total sums up to a value equal or larger than maxRequestCount_
     * the operation is no longer allowed and false is returned; true is
     * returned otherwise.
     *
     * @param ip
     * @return true
     * @return false
     */
    [[maybe_unused]] bool
    request(std::string const& ip) noexcept
    {
        if (whitelistHandler_.isWhiteListed(ip))
            return true;

        {
            std::scoped_lock lck(mtx_);
            ipState_[ip].requestsCount++;
        }

        return isOk(ip);
    }

    /**
     * @brief Instantly clears all fetch counters added by @see add(std::string
     * const&, uint32_t)
     */
    void
    clear() noexcept override
    {
        std::scoped_lock lck(mtx_);
        ipState_.clear();
    }

private:
    [[nodiscard]] std::unordered_set<std::string> const
    getWhitelist(clio::Config const& config) const
    {
        using T = std::unordered_set<std::string> const;
        auto whitelist = config.arrayOr("dos_guard.whitelist", {});
        auto const transform = [](auto const& elem) { return elem.template value<std::string>(); };
        return T{
            boost::transform_iterator(std::begin(whitelist), transform),
            boost::transform_iterator(std::end(whitelist), transform)};
    }
};

/**
 * @brief Sweep handler using a steady_timer and boost::asio::io_context.
 */
class IntervalSweepHandler
{
    std::chrono::milliseconds sweepInterval_;
    std::reference_wrapper<boost::asio::io_context> ctx_;
    BaseDOSGuard* dosGuard_ = nullptr;

    boost::asio::steady_timer timer_{boost::asio::make_strand(ctx_.get())};

public:
    /**
     * @brief Construct a new interval-based sweep handler
     *
     * @param config Clio config
     * @param ctx The boost::asio::io_context
     */
    IntervalSweepHandler(clio::Config const& config, boost::asio::io_context& ctx)
        : sweepInterval_{std::max(1u, static_cast<uint32_t>(config.valueOr("dos_guard.sweep_interval", 1.0) * 1000.0))}
        , ctx_{std::ref(ctx)}
    {
    }

    ~IntervalSweepHandler()
    {
        timer_.cancel();
    }

    /**
     * @brief This setup member function is called by @ref BasicDOSGuard during
     * its initialization.
     *
     * @param guard Pointer to the dos guard
     */
    void
    setup(BaseDOSGuard* guard)
    {
        assert(dosGuard_ == nullptr);
        dosGuard_ = guard;
        assert(dosGuard_ != nullptr);

        createTimer();
    }

private:
    void
    createTimer()
    {
        timer_.expires_after(sweepInterval_);
        timer_.async_wait([this](boost::system::error_code const& error) {
            if (error == boost::asio::error::operation_aborted)
                return;

            dosGuard_->clear();
            createTimer();
        });
    }
};

using DOSGuard = BasicDOSGuard<WhitelistHandler, IntervalSweepHandler>;

}  // namespace clio
