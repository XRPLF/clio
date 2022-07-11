#ifndef RIPPLE_REPORTING_DOS_GUARD_H
#define RIPPLE_REPORTING_DOS_GUARD_H

#include <boost/asio.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

class DOSGuard
{
    boost::asio::io_context& ctx_;
    std::mutex mtx_;  // protects ipFetchCount_
    std::unordered_map<std::string, std::uint32_t> ipFetchCount_;
    std::unordered_set<std::string> const whitelist_;
    std::uint32_t const maxFetches_;
    std::uint32_t const sweepInterval_;

    // Load config setting for DOSGuard
    std::optional<boost::json::object>
    getConfig(boost::json::object const& config) const
    {
        if (!config.contains("dos_guard"))
            return {};

        return config.at("dos_guard").as_object();
    }

    std::uint32_t
    get(boost::json::object const& config,
        std::string const& key,
        std::uint32_t const fallback) const
    {
        try
        {
            if (auto const c = getConfig(config))
                return c->at(key).as_int64();
        }
        catch (std::exception const& e)
        {
        }

        return fallback;
    }

    std::unordered_set<std::string> const
    getWhitelist(boost::json::object const& config) const
    {
        using T = std::unordered_set<std::string> const;

        try
        {
            auto const& c = getConfig(config);
            if (!c)
                return T();

            auto const& w = c->at("whitelist").as_array();

            auto const transform = [](auto const& elem) {
                return std::string(elem.as_string().c_str());
            };

            return T(
                boost::transform_iterator(w.begin(), transform),
                boost::transform_iterator(w.end(), transform));
        }
        catch (std::exception const& e)
        {
            return T();
        }
    }

public:
    DOSGuard(boost::json::object const& config, boost::asio::io_context& ctx)
        : ctx_(ctx)
        , whitelist_(getWhitelist(config))
        , maxFetches_(get(config, "max_fetches", 100))
        , sweepInterval_(get(config, "sweep_interval", 1))
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
        auto it = ipFetchCount_.find(ip);
        if (it == ipFetchCount_.end())
            return true;

        return it->second < maxFetches_;
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
};
#endif
