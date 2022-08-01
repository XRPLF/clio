#include <boost/asio.hpp>
#include <clio/main/Application.h>
#include <clio/webserver/DOSGuard.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

DOSGuard::DOSGuard(Application const& app)
    : app_(app)
    , ctx_(app_.rpc())
    , whitelist_(app_.config().dosGuard.whitelist)
    , maxFetches_(app_.config().dosGuard.maxFetches)
    , sweepInterval_(app.config().dosGuard.maxFetches)
{
    createTimer();
}

void
DOSGuard::createTimer()
{
    auto wait = std::chrono::seconds(sweepInterval_);
    std::shared_ptr<boost::asio::steady_timer> timer =
        std::make_shared<boost::asio::steady_timer>(
            ctx_, std::chrono::steady_clock::now() + wait);
    timer->async_wait([timer, this](const boost::system::error_code& error) {
        clear();
        createTimer();
    });
}

bool
DOSGuard::isOk(std::string const& ip)
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
DOSGuard::add(std::string const& ip, uint32_t numObjects)
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
DOSGuard::clear()
{
    std::unique_lock lck(mtx_);
    ipFetchCount_.clear();
}

bool
DOSGuard::isWhiteListed(std::string const& ip)
{
    return whitelist_.contains(ip);
}