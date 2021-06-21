#include <boost/asio.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

class DOSGuard
{
    std::unordered_map<std::string, uint32_t> ipFetchCount_;
    uint32_t maxFetches_ = 100;
    uint32_t sweepInterval_ = 1;
    std::unordered_set<std::string> whitelist_;
    boost::asio::io_context& ctx_;
    std::mutex mtx_;

public:
    DOSGuard(boost::json::object const& config, boost::asio::io_context& ctx)
        : ctx_(ctx)
    {
        if (config.contains("dos_guard"))
        {
            auto dosGuardConfig = config.at("dos_guard").as_object();
            if (dosGuardConfig.contains("max_fetches") &&
                dosGuardConfig.contains("sweep_interval"))
            {
                maxFetches_ = dosGuardConfig.at("max_fetches").as_int64();
                sweepInterval_ = dosGuardConfig.at("sweep_interval").as_int64();
            }
            if (dosGuardConfig.contains("whitelist"))
            {
                auto whitelist = dosGuardConfig.at("whitelist").as_array();
                for (auto& ip : whitelist)
                    whitelist_.insert(ip.as_string().c_str());
            }
        }
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
    isOk(std::string const& ip)
    {
        if (whitelist_.count(ip) > 0)
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
        if (whitelist_.count(ip) > 0)
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
