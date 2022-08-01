#ifndef RIPPLE_REPORTING_DOS_GUARD_H
#define RIPPLE_REPORTING_DOS_GUARD_H

#include <boost/asio.hpp>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Application;

class DOSGuard
{
    Application const& app_;
    boost::asio::io_context& ctx_;

    std::mutex mtx_;  // protects ipFetchCount_
    std::unordered_map<std::string, std::uint32_t> ipFetchCount_ = {};
    std::unordered_set<std::string> const& whitelist_;
    std::uint32_t const maxFetches_;
    std::uint32_t const sweepInterval_;

public:
    DOSGuard(Application const& app);

    void
    createTimer();

    bool
    isWhiteListed(std::string const& ip);

    bool
    isOk(std::string const& ip);

    bool
    add(std::string const& ip, uint32_t numObjects);

    void
    clear();
};
#endif  // RIPPLE_REPORTING_DOS_GUARD_H
