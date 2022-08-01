#ifndef REPORTING_RPC_H_INCLUDED
#define REPORTING_RPC_H_INCLUDED

#include <ripple/protocol/ErrorCodes.h>
#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <clio/backend/BackendInterface.h>
#include <clio/main/Application.h>
#include <clio/rpc/Counters.h>
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

struct Context
{
    std::string method;
    std::uint32_t version;
    boost::json::object const& params;
    Application const& app;
    std::shared_ptr<WsBase> session;
    Backend::LedgerRange const& range;
    std::string clientIp;
    boost::asio::yield_context& yield;

    Context(
        std::string const& command_,
        std::uint32_t version_,
        boost::json::object const& params_,
        Application const& app_,
        std::shared_ptr<WsBase> const& session_,
        Backend::LedgerRange const& range_,
        std::string const& clientIp_,
        boost::asio::yield_context& yield_)
        : method(command_)
        , version(version_)
        , params(params_)
        , app(app_)
        , session(session_)
        , range(range_)
        , clientIp(clientIp_)
        , yield(yield_)
    {
    }
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

bool
shouldForwardToRippled(Context const& ctx);

boost::json::object
make_error(Status const& status);

boost::json::object
make_error(Error err);

std::optional<Context>
make_WsContext(
    boost::json::object const& request,
    Application const& app,
    std::shared_ptr<WsBase> const& session,
    Backend::LedgerRange const& range,
    std::string const& clientIp,
    boost::asio::yield_context& yield);

std::optional<Context>
make_HttpContext(
    boost::json::object const& request,
    Application const& app,
    Backend::LedgerRange const& range,
    std::string const& clientIp,
    boost::asio::yield_context& yield);

Result
buildResponse(Context const& ctx);

bool
validHandler(std::string const& method);

Status
getLimit(RPC::Context const& context, std::uint32_t& limit);

template <class T>
void
logDuration(Context const& ctx, T const& dur)
{
    std::stringstream ss;
    ss << "Request processing duration = "
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
