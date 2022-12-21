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

#include <backend/BackendInterface.h>
#include <rpc/Counters.h>
#include <rpc/Errors.h>
#include <util/Taggable.h>
#include <util/log/Logger.h>

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>

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

namespace clio {
namespace etl {
class ETLLoadBalancer;
class ReportingETL;
}  // namespace etl
namespace web {
class WsBase;
}  // namespace web
namespace subscription {
class SubscriptionManager;
}  // namespace subscription

namespace rpc {

struct Context : public util::Taggable
{
    util::Logger perfLog_{"Performance"};
    boost::asio::yield_context& yield;
    std::string method;
    std::uint32_t version;
    boost::json::object const& params;
    std::shared_ptr<BackendInterface const> const& backend;
    // this needs to be an actual shared_ptr, not a reference. The above
    // references refer to shared_ptr members of WsBase, but WsBase contains
    // SubscriptionManager as a weak_ptr, to prevent a shared_ptr cycle.
    std::shared_ptr<subscription::SubscriptionManager> subscriptions;
    std::shared_ptr<etl::ETLLoadBalancer> const& balancer;
    std::shared_ptr<etl::ReportingETL const> const& etl;
    std::shared_ptr<web::WsBase> session;
    data::LedgerRange const& range;
    Counters& counters;
    std::string clientIp;

    Context(
        boost::asio::yield_context& yield_,
        std::string const& command_,
        std::uint32_t version_,
        boost::json::object const& params_,
        std::shared_ptr<BackendInterface const> const& backend_,
        std::shared_ptr<subscription::SubscriptionManager> const&
            subscriptions_,
        std::shared_ptr<etl::ETLLoadBalancer> const& balancer_,
        std::shared_ptr<etl::ReportingETL const> const& etl_,
        std::shared_ptr<web::WsBase> const& session_,
        util::TagDecoratorFactory const& tagFactory_,
        data::LedgerRange const& range_,
        Counters& counters_,
        std::string const& clientIp_);
};

struct AccountCursor
{
    ripple::uint256 index;
    std::uint32_t hint;

    std::string
    toString() const
    {
        return ripple::strHex(index) + "," + std::to_string(hint);
    }

    bool
    isNonZero() const
    {
        return index.isNonZero() || hint != 0;
    }
};

using Result = std::variant<Status, boost::json::object>;

std::optional<Context>
make_WsContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    std::shared_ptr<BackendInterface const> const& backend,
    std::shared_ptr<subscription::SubscriptionManager> const& subscriptions,
    std::shared_ptr<etl::ETLLoadBalancer> const& balancer,
    std::shared_ptr<etl::ReportingETL const> const& etl,
    std::shared_ptr<web::WsBase> const& session,
    util::TagDecoratorFactory const& tagFactory,
    data::LedgerRange const& range,
    Counters& counters,
    std::string const& clientIp);

std::optional<Context>
make_HttpContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    std::shared_ptr<BackendInterface const> const& backend,
    std::shared_ptr<subscription::SubscriptionManager> const& subscriptions,
    std::shared_ptr<etl::ETLLoadBalancer> const& balancer,
    std::shared_ptr<etl::ReportingETL const> const& etl,
    util::TagDecoratorFactory const& tagFactory,
    data::LedgerRange const& range,
    Counters& counters,
    std::string const& clientIp);

Result
buildResponse(Context const& ctx);

bool
validHandler(std::string const& method);

bool
isClioOnly(std::string const& method);

Status
getLimit(rpc::Context const& context, std::uint32_t& limit);

template <class T>
void
logDuration(Context const& ctx, T const& dur)
{
    static util::Logger log{"RPC"};
    std::stringstream ss;
    ss << ctx.tag() << "Request processing duration = "
       << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count()
       << " milliseconds. request = " << ctx.params;
    auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(dur).count();
    if (seconds > 10)
        log.error() << ss.str();
    else if (seconds > 1)
        log.warn() << ss.str();
    else
        log.info() << ss.str();
}

}  // namespace rpc
}  // namespace clio
