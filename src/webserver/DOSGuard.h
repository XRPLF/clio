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
    std::shared_mutex ftMtx_;   // protects ipFetchCount_
    std::shared_mutex reqMtx_;  // protects ipRequestCount_
    std::unordered_map<std::string, std::atomic_uint32_t> ipFetchCount_;
    std::unordered_map<std::string, std::atomic_uint32_t> ipRequestCount_;
    std::unordered_set<std::string> const whitelist_;
    std::uint32_t const maxFetches_;
    std::uint32_t const sweepInterval_;
    std::uint32_t const maxConcurrentRequests_;

    struct RPCScope
    {
        std::uint32_t count_;
        std::string ip_;
        DOSGuard& parent_;

        RPCScope(std::string ip, DOSGuard& parent) : ip_(ip), parent_(parent)
        {
            count_ = parent_.incrementRequestCount(ip_);
        }

        ~RPCScope()
        {
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

    std::uint32_t
    getFetches(std::string const& ip)
    {
        std::shared_lock lk(ftMtx_);
        auto it = ipFetchCount_.find(ip);
        if (it == ipFetchCount_.end())
            return 0;
        else
            return it->second;
    }

    std::uint32_t
    getRequests(std::string const& ip)
    {
        std::shared_lock lk(reqMtx_);
        auto it = ipRequestCount_.find(ip);
        if (it == ipRequestCount_.end())
            return 0;
        else
            return it->second;
    }

    // increment atomic request counter for ip in a threadsafe manner.
    // returns result (not a reference)
    std::uint32_t
    incrementRequestCount(std::string const& ip)
    {
        // reading ipReqiestCount_[ip] can potentially mutate the internal
        // ipRequestCount_ data structure. One of two things will happen:
        // 1) ipRequestCount_[ip] contains a reference to an atomic
        // 2) ipRequestCount_[ip] creates a new reference
        //      -this mutates the data structure

        {
            std::shared_lock read_lock(reqMtx_);
            auto it = ipRequestCount_.find(ip);
            if (it != ipRequestCount_.end())
                return ipRequestCount_[ip]++;
        }
        {
            // at this point the ip does not exist in the map. Must grab
            // writer lock
            std::unique_lock write_lock(reqMtx_);
            // this will create and increment the atomic OR
            // increment the atomic if another thread succeeded for this ip.
            return ipRequestCount_[ip]++;
        }
    }

    void
    decrementRequestCount(std::string const& ip)
    {
        // assuming that ip is in the map
        {
            std::shared_lock read_lock(reqMtx_);
            auto it = ipRequestCount_.find(ip);
            if (it == ipRequestCount_.end())
                assert(false);
            if (--ipRequestCount_[ip])
                return;
        }
        // erase if count is still zero
        {
            std::unique_lock write_lock(reqMtx_);
            auto it = ipRequestCount_.find(ip);
            if (it == ipRequestCount_.end())
                return;  // another thread erased the reference

            if (!it->second)  // if still zero
                ipRequestCount_.erase(ip);
        }
        return;
    }

    bool
    isOk(std::string const& ip, std::uint32_t requestCount)
    {
        if (whitelist_.contains(ip))
            return true;
        std::shared_lock lk(ftMtx_);
        bool fetchesOk = maxFetches_ == -1 || getFetches(ip) < maxFetches_;
        bool requestsOk = maxConcurrentRequests_ == -1 ||
            requestCount < maxConcurrentRequests_;
        return fetchesOk && requestsOk;
    }

public:
    DOSGuard(boost::json::object const& config, boost::asio::io_context& ctx)
        : ctx_(ctx)
        , whitelist_(getWhitelist(config))
        , maxFetches_(get(config, "max_fetches", 100))
        , sweepInterval_(get(config, "sweep_interval", 1))
        , maxConcurrentRequests_(get(config, "max_concurrent_requests", 4))
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
        std::shared_lock flk(ftMtx_);
        std::shared_lock rlk(reqMtx_);
        bool fetchesOk = maxFetches_ == -1 || getFetches(ip) < maxFetches_;
        bool requestsOk = maxConcurrentRequests_ == -1 ||
            getRequests(ip) < maxConcurrentRequests_;
        return fetchesOk && requestsOk;
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
        if (whitelist_.contains(ip) || maxFetches_ == -1)
            return true;
        {
            std::unique_lock lck(ftMtx_);
            auto it = ipFetchCount_.find(ip);
            if (it == ipFetchCount_.end())
                ipFetchCount_[ip] = numObjects;
            else
                it->second += numObjects;
            return ipFetchCount_[ip] < maxFetches_;
        }
    }

    void
    clear()
    {
        std::unique_lock lck(ftMtx_);
        ipFetchCount_.clear();
    }
};
#endif
