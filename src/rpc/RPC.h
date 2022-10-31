#ifndef REPORTING_RPC_H_INCLUDED
#define REPORTING_RPC_H_INCLUDED

#include <ripple/protocol/ErrorCodes.h>

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>

#include <backend/BackendInterface.h>
#include <rpc/Counters.h>
#include <util/Taggable.h>

#include <optional>
#include <string>
#include <variant>

/*
 * This file contains various classes necessary for executing RPC handlers.
 * Context gives the handlers access to various other parts of the application
 * Status is used to report errors.
 * And lastly, there are various functions for making Contexts, Statuses and
 * serializing Status to JSON.
 * This file is meant to contain any class or function that code outside of the
 * rpc folder needs to use. For helper functions or classes used within the rpc
 * folder, use RPCHelpers.h.
 */

class WsBase;
class SubscriptionManager;
class ETLLoadBalancer;
class ReportingETL;

namespace RPC {

struct Context : public util::Taggable
{
    boost::asio::yield_context& yield;
    std::string method;
    std::uint32_t version;
    boost::json::object const& params;
    std::shared_ptr<BackendInterface const> const& backend;
    // this needs to be an actual shared_ptr, not a reference. The above
    // references refer to shared_ptr members of WsBase, but WsBase contains
    // SubscriptionManager as a weak_ptr, to prevent a shared_ptr cycle.
    std::shared_ptr<SubscriptionManager> subscriptions;
    std::shared_ptr<ETLLoadBalancer> const& balancer;
    std::shared_ptr<ReportingETL const> const& etl;
    std::shared_ptr<WsBase> session;
    Backend::LedgerRange const& range;
    Counters& counters;
    std::string clientIp;

    Context(
        boost::asio::yield_context& yield_,
        std::string const& command_,
        std::uint32_t version_,
        boost::json::object const& params_,
        std::shared_ptr<BackendInterface const> const& backend_,
        std::shared_ptr<SubscriptionManager> const& subscriptions_,
        std::shared_ptr<ETLLoadBalancer> const& balancer_,
        std::shared_ptr<ReportingETL const> const& etl_,
        std::shared_ptr<WsBase> const& session_,
        util::TagDecoratorFactory const& tagFactory_,
        Backend::LedgerRange const& range_,
        Counters& counters_,
        std::string const& clientIp_);
};
using Error = ripple::error_code_i;

struct AccountCursor
{
    ripple::uint256 index;
    std::uint32_t hint;

    std::string
    toString()
    {
        return ripple::strHex(index) + "," + std::to_string(hint);
    }

    bool
    isNonZero()
    {
        return index.isNonZero() || hint != 0;
    }
};

struct Status
{
    Error error = Error::rpcSUCCESS;
    std::string strCode = "";
    std::string message = "";

    Status(){};

    Status(Error error_) : error(error_){};

    // HACK. Some rippled handlers explicitly specify errors.
    // This means that we have to be able to duplicate this
    // functionality.
    Status(std::string const& message_)
        : error(ripple::rpcUNKNOWN), message(message_)
    {
    }

    Status(Error error_, std::string message_)
        : error(error_), message(message_)
    {
    }

    Status(Error error_, std::string strCode_, std::string message_)
        : error(error_), strCode(strCode_), message(message_)
    {
    }

    /** Returns true if the Status is *not* OK. */
    operator bool() const
    {
        return error != Error::rpcSUCCESS;
    }
};

static Status OK;

using Result = std::variant<Status, boost::json::object>;

class InvalidParamsError : public std::exception
{
    std::string msg;

public:
    InvalidParamsError(std::string const& msg) : msg(msg)
    {
    }

    const char*
    what() const throw() override
    {
        return msg.c_str();
    }
};
class AccountNotFoundError : public std::exception
{
    std::string account;

public:
    AccountNotFoundError(std::string const& acct) : account(acct)
    {
    }
    const char*
    what() const throw() override
    {
        return account.c_str();
    }
};

enum warning_code {
    warnUNKNOWN = -1,
    warnRPC_CLIO = 2001,
    warnRPC_OUTDATED = 2002,
    warnRPC_RATE_LIMIT = 2003
};

struct WarningInfo
{
    constexpr WarningInfo() : code(warnUNKNOWN), message("unknown warning")
    {
    }

    constexpr WarningInfo(warning_code code_, char const* message_)
        : code(code_), message(message_)
    {
    }
    warning_code code;
    std::string_view const message;
};

WarningInfo const&
get_warning_info(warning_code code);

boost::json::object
make_warning(warning_code code);

boost::json::object
make_error(Status const& status);

boost::json::object
make_error(Error err);

std::optional<Context>
make_WsContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    std::shared_ptr<BackendInterface const> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptions,
    std::shared_ptr<ETLLoadBalancer> const& balancer,
    std::shared_ptr<ReportingETL const> const& etl,
    std::shared_ptr<WsBase> const& session,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    Counters& counters,
    std::string const& clientIp);

std::optional<Context>
make_HttpContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    std::shared_ptr<BackendInterface const> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptions,
    std::shared_ptr<ETLLoadBalancer> const& balancer,
    std::shared_ptr<ReportingETL const> const& etl,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    Counters& counters,
    std::string const& clientIp);

Result
buildResponse(Context const& ctx);

bool
validHandler(std::string const& method);

bool
isClioOnly(std::string const& method);

Status
getLimit(RPC::Context const& context, std::uint32_t& limit);

template <class T>
void
logDuration(Context const& ctx, T const& dur)
{
    std::stringstream ss;
    ss << ctx.tag() << "Request processing duration = "
       << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()
       << " milliseconds. request = " << ctx.params;
    auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(dur).count();
    if (seconds > 10)
        BOOST_LOG_TRIVIAL(error) << ss.str();
    else if (seconds > 1)
        BOOST_LOG_TRIVIAL(warning) << ss.str();
    else
        BOOST_LOG_TRIVIAL(info) << ss.str();
}

}  // namespace RPC

#endif  // REPORTING_RPC_H_INCLUDED
