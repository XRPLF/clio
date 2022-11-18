#ifndef RIPPLE_REPORTING_DOS_GUARD_H
#define RIPPLE_REPORTING_DOS_GUARD_H

#include <boost/asio.hpp>
#include <atomic>
#include <string>
#include <unordered_map>
#include <unordered_set>

class DOSGuard
{
    boost::asio::io_context& ctx_;
    mutable std::shared_mutex fetchMtx_;  // protects ipFetchCount_
    mutable std::shared_mutex reqMtx_;    // protects ipRequestCount_
    std::unordered_map<std::string, uint32_t> ipFetchCount_;
    std::unordered_map<std::string, uint32_t> ipRequestCount_;
    std::unordered_set<std::string> const whitelist_;
    std::optional<uint32_t> const maxFetches_;
    std::optional<uint32_t> const maxConcurrentRequests_;
    std::optional<uint32_t> const sweepInterval_;
    // We track the time we swept so that way a request that started before the
    // sweep but finished after doesn't decrement the request count
    std::chrono::time_point<std::chrono::system_clock> lastSweep_;

    struct RPCScope
    {
        std::uint32_t count_;
        std::string ip_;
        DOSGuard& parent_;
        std::chrono::time_point<std::chrono::system_clock> lastSweep_;

        RPCScope(std::string ip, DOSGuard& parent) : ip_(ip), parent_(parent)
        {
            auto [cnt, sweep] = parent_.incrementRequestCount(ip_);
            count_ = cnt;
            lastSweep_ = sweep;
        }

        RPCScope(RPCScope&& other) = default;
        RPCScope(RPCScope const& other) = default;

        ~RPCScope()
        {
            // don't decrement if we swept since starting the request
            if (lastSweep_ == parent_.lastSweep_)
                parent_.decrementRequestCount(ip_);
        }

        bool
        isValid()
        {
            return parent_.isOk(ip_, count_);
        }
    };

    // Load config setting for DOSGuard
    std::optional<boost::json::object>
    getConfig(boost::json::object const& config) const
    {
        if (!config.contains("dos_guard"))
            return {};

        return config.at("dos_guard").as_object();
    }

    std::optional<std::uint32_t>
    get(boost::json::object const& config,
        std::string const& key,
        std::uint32_t const fallback) const
    {
        try
        {
            if (auto const c = getConfig(config))
            {
                auto v = c->at(key).as_int64();
                if (v == -1)
                    return {};
                return v;
            }
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

    std::uint32_t
    getFetches(std::string const& ip) const
    {
        std::shared_lock lk{fetchMtx_};
        auto const it = ipFetchCount_.find(ip);
        if (it == ipFetchCount_.end())
            return 0;
        else
            return it->second;
    }

    std::uint32_t
    getRequests(std::string const& ip) const
    {
        std::shared_lock lk{reqMtx_};
        auto const it = ipRequestCount_.find(ip);
        if (it == ipRequestCount_.end())
            return 0;
        else
            return it->second;
    }

    std::pair<std::uint32_t, std::chrono::time_point<std::chrono::system_clock>>
    incrementRequestCount(std::string const& ip)
    {
        if (!maxConcurrentRequests_)
            return {
                0, std::chrono::system_clock::now()};  // doesn't matter what we
                                                       // return here
        std::unique_lock lck{reqMtx_};
        // need to return both of these under the lock
        return {++ipRequestCount_[ip], lastSweep_};
    }

    void
    decrementRequestCount(std::string const& ip)
    {
        if (!maxConcurrentRequests_)
            return;
        std::unique_lock lck{reqMtx_};
        auto it = ipRequestCount_.find(ip);
        if (it != ipRequestCount_.end() && it->second != 0)
            --it->second;
    }
    bool
    isOk(std::string const& ip, std::uint32_t requestCount) const
    {
        if (whitelist_.contains(ip))
            return true;
        bool fetchesOk = !maxFetches_ || getFetches(ip) <= maxFetches_;
        bool requestsOk =
            !maxConcurrentRequests_ || requestCount <= *maxConcurrentRequests_;
        return fetchesOk && requestsOk;
    }

    void
    createTimer()
    {
        assert(sweepInterval_);
        auto wait = std::chrono::seconds(*sweepInterval_);
        std::shared_ptr<boost::asio::steady_timer> timer =
            std::make_shared<boost::asio::steady_timer>(
                ctx_, std::chrono::steady_clock::now() + wait);
        timer->async_wait(
            [timer, this](const boost::system::error_code& error) {
                clear();
                createTimer();
            });
    }

    void
    clear()
    {
        if (maxFetches_)
        {
            std::unique_lock lck{fetchMtx_};
            ipFetchCount_.clear();
        }

        if (maxConcurrentRequests_)
        {
            std::unique_lock lck{reqMtx_};
            // set time first, else requests might still decrement after the
            // sweep
            lastSweep_ = std::chrono::system_clock::now();
            ipRequestCount_.clear();
        }
    }

public:
    DOSGuard(boost::json::object const& config, boost::asio::io_context& ctx)
        : ctx_(ctx)
        , whitelist_(getWhitelist(config))
        , maxFetches_(get(config, "max_fetches", 100000000))  // 100 MB
        , maxConcurrentRequests_(get(config, "max_concurrent_requests", 1))
        , sweepInterval_(get(config, "sweep_interval", 10))
        , lastSweep_(std::chrono::system_clock::now())
    {
        if (sweepInterval_)
            createTimer();
    }

    bool
    isWhiteListed(std::string const& ip)
    {
        return whitelist_.contains(ip);
    }

    RPCScope
    checkout(std::string const& ip)
    {
        return RPCScope(ip, *this);
    }

    // add numObjects bytes to the fetch count for ip
    bool
    add(std::string const& ip, uint32_t numObjects)
    {
        if (whitelist_.contains(ip) || !maxFetches_)
            return true;
        {
            std::unique_lock lck{fetchMtx_};
            auto it = ipFetchCount_.find(ip);
            uint32_t count = 0;
            if (it == ipFetchCount_.end())
                count = ipFetchCount_[ip] = numObjects;
            else
                count = (it->second += numObjects);
            return count < maxFetches_;
        }
    }
};
#endif
