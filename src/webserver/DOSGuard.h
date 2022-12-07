#pragma once

#include <boost/asio.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <config/Config.h>

class DOSGuard
{
    boost::asio::io_context& ctx_;
    std::mutex mtx_;  // protects ipFetchCount_
    std::unordered_map<std::string, std::uint32_t> ipFetchCount_;
    std::unordered_map<std::string, std::uint32_t> ipConnCount_;
    std::unordered_set<std::string> const whitelist_;
    std::uint32_t const maxFetches_;
    std::uint32_t const sweepInterval_;
    std::uint32_t const maxConnCount_;

public:
    DOSGuard(clio::Config const& config, boost::asio::io_context& ctx)
        : ctx_{ctx}
        , whitelist_{getWhitelist(config)}
        , maxFetches_{config.valueOr("dos_guard.max_fetches", 100000000u)}
        , sweepInterval_{config.valueOr("dos_guard.sweep_interval", 10u)}
        , maxConnCount_{config.valueOr("dos_guard.max_connections", 1u)}
    {
        createTimer();
    }

    void
    createTimer()
    {
        auto wait = std::chrono::seconds(sweepInterval_);
        std::shared_ptr<boost::asio::steady_timer> timer =
            std::make_shared<boost::asio::steady_timer>(
                ctx_, std::chrono::steady_clock::now() + wait);
        timer->async_wait(
            [timer, this](const boost::system::error_code& error) {
                clear();
                createTimer();
            });
    }

    bool
    isWhiteListed(std::string const& ip)
    {
        return whitelist_.contains(ip);
    }

    bool
    isOk(std::string const& ip)
    {
        if (whitelist_.contains(ip))
            return true;

        std::unique_lock lck(mtx_);
        bool fetchesOk = true;
        bool connsOk = true;
        {
            auto it = ipFetchCount_.find(ip);
            if (it != ipFetchCount_.end())
                fetchesOk = it->second <= maxFetches_;
        }
        {
            auto it = ipConnCount_.find(ip);
            assert(it != ipConnCount_.end());
            if (it != ipConnCount_.end())
            {
                connsOk = it->second <= maxConnCount_;
            }
        }

        return fetchesOk && connsOk;
    }

    void
    increment(std::string const& ip)
    {
        if (whitelist_.contains(ip))
            return;
        std::unique_lock lck{mtx_};
        ipConnCount_[ip]++;
    }

    void
    decrement(std::string const& ip)
    {
        if (whitelist_.contains(ip))
            return;
        std::unique_lock lck{mtx_};
        assert(ipConnCount_[ip] > 0);
        ipConnCount_[ip]--;
        if (ipConnCount_[ip] == 0)
            ipConnCount_.erase(ip);
    }

    bool
    add(std::string const& ip, uint32_t numObjects)
    {
        if (whitelist_.contains(ip))
            return true;

        {
            std::unique_lock lck(mtx_);
            auto it = ipFetchCount_.find(ip);
            if (it == ipFetchCount_.end())
                ipFetchCount_[ip] = numObjects;
            else
                it->second += numObjects;
        }

        return isOk(ip);
    }

    void
    clear()
    {
        std::unique_lock lck(mtx_);
        ipFetchCount_.clear();
    }

private:
    std::unordered_set<std::string> const
    getWhitelist(clio::Config const& config) const
    {
        using T = std::unordered_set<std::string> const;
        auto whitelist = config.arrayOr("dos_guard.whitelist", {});
        auto const transform = [](auto const& elem) {
            return elem.template value<std::string>();
        };
        return T{
            boost::transform_iterator(std::begin(whitelist), transform),
            boost::transform_iterator(std::end(whitelist), transform)};
    }
};
