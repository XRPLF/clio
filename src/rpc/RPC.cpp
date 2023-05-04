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

#include <etl/ETLSource.h>
#include <log/Logger.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/impl/HandlerProvider.h>
#include <webserver/HttpBase.h>
#include <webserver/WsBase.h>

#include <boost/asio/spawn.hpp>

#include <unordered_map>

using namespace std;
using namespace clio;
using namespace RPC;

// local to compilation unit loggers
namespace {
clio::Logger gPerfLog{"Performance"};
clio::Logger gLog{"RPC"};
}  // namespace

namespace RPC {

optional<Web::Context>
make_WsContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    shared_ptr<WsBase> const& session,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    string const& clientIp)
{
    boost::json::value commandValue = nullptr;
    if (!request.contains("command") && request.contains("method"))
        commandValue = request.at("method");
    else if (request.contains("command") && !request.contains("method"))
        commandValue = request.at("command");

    if (!commandValue.is_string())
        return {};

    string command = commandValue.as_string().c_str();
    return make_optional<Web::Context>(yc, command, 1, request, session, tagFactory, range, clientIp);
}

optional<Web::Context>
make_HttpContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    string const& clientIp)
{
    if (!request.contains("method") || !request.at("method").is_string())
        return {};

    string const& command = request.at("method").as_string().c_str();

    if (command == "subscribe" || command == "unsubscribe")
        return {};

    if (!request.at("params").is_array())
        return {};

    boost::json::array const& array = request.at("params").as_array();

    if (array.size() != 1)
        return {};

    if (!array.at(0).is_object())
        return {};

    return make_optional<Web::Context>(yc, command, 1, array.at(0).as_object(), nullptr, tagFactory, range, clientIp);
}

static unordered_set<string> forwardCommands{
    "submit",
    "submit_multisigned",
    "fee",
    "ledger_closed",
    "ledger_current",
    "ripple_path_find",
    "manifest",
    "channel_authorize",
    "channel_verify"};

RPCEngine::RPCEngine(
    std::shared_ptr<BackendInterface> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptions,
    std::shared_ptr<ETLLoadBalancer> const& balancer,
    std::shared_ptr<ReportingETL> const& etl,
    clio::DOSGuard const& dosGuard,
    WorkQueue& workQueue,
    Counters& counters,
    std::shared_ptr<HandlerProvider const> const& handlerProvider)
    : backend_{backend}
    , subscriptions_{subscriptions}
    , balancer_{balancer}
    , dosGuard_{std::cref(dosGuard)}
    , workQueue_{std::ref(workQueue)}
    , counters_{std::ref(counters)}
    , handlerTable_{handlerProvider}
{
}

std::shared_ptr<RPCEngine>
RPCEngine::make_RPCEngine(
    clio::Config const& config,
    std::shared_ptr<BackendInterface> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptions,
    std::shared_ptr<ETLLoadBalancer> const& balancer,
    std::shared_ptr<ReportingETL> const& etl,
    clio::DOSGuard const& dosGuard,
    WorkQueue& workQueue,
    Counters& counters,
    std::shared_ptr<HandlerProvider const> const& handlerProvider)
{
    return std::make_shared<RPCEngine>(
        backend, subscriptions, balancer, etl, dosGuard, workQueue, counters, handlerProvider);
}

bool
RPCEngine::validHandler(string const& method) const
{
    return handlerTable_.contains(method) || forwardCommands.contains(method);
}

bool
RPCEngine::isClioOnly(string const& method) const
{
    return handlerTable_.isClioOnly(method);
}

bool
RPCEngine::shouldForwardToRippled(Web::Context const& ctx) const
{
    auto request = ctx.params;

    if (isClioOnly(ctx.method))
        return false;

    if (forwardCommands.find(ctx.method) != forwardCommands.end())
        return true;

    if (specifiesCurrentOrClosedLedger(request))
        return true;

    if (ctx.method == "account_info" && request.contains("queue") && request.at("queue").as_bool())
        return true;

    return false;
}

Result
RPCEngine::buildResponse(Web::Context const& ctx)
{
    if (shouldForwardToRippled(ctx))
    {
        auto toForward = ctx.params;
        toForward["command"] = ctx.method;

        auto const res = balancer_->forwardToRippled(toForward, ctx.clientIp, ctx.yield);
        notifyForwarded(ctx.method);

        if (!res)
            return Status{RippledError::rpcFAILED_TO_FORWARD};

        return *res;
    }

    if (backend_->isTooBusy())
    {
        gLog.error() << "Database is too busy. Rejecting request";
        return Status{RippledError::rpcTOO_BUSY};
    }

    auto const method = handlerTable_.getHandler(ctx.method);
    if (!method)
        return Status{RippledError::rpcUNKNOWN_COMMAND};

    try
    {
        gPerfLog.debug() << ctx.tag() << " start executing rpc `" << ctx.method << '`';

        auto const isAdmin = ctx.clientIp == "127.0.0.1";  // TODO: this should be a strategy
        auto const context = Context{ctx.yield, ctx.session, isAdmin, ctx.clientIp};
        auto const v = (*method).process(ctx.params, context);

        gPerfLog.debug() << ctx.tag() << " finish executing rpc `" << ctx.method << '`';

        if (v)
            return v->as_object();
        else
            return Status{v.error()};
    }
    catch (InvalidParamsError const& err)
    {
        return Status{RippledError::rpcINVALID_PARAMS, err.what()};
    }
    catch (AccountNotFoundError const& err)
    {
        return Status{RippledError::rpcACT_NOT_FOUND, err.what()};
    }
    catch (Backend::DatabaseTimeout const& t)
    {
        gLog.error() << "Database timeout";
        return Status{RippledError::rpcTOO_BUSY};
    }
    catch (exception const& err)
    {
        gLog.error() << ctx.tag() << " caught exception: " << err.what();
        return Status{RippledError::rpcINTERNAL};
    }
}

void
RPCEngine::notifyComplete(std::string const& method, std::chrono::microseconds const& duration)
{
    if (validHandler(method))
        counters_.get().rpcComplete(method, duration);
}

void
RPCEngine::notifyErrored(std::string const& method)
{
    if (validHandler(method))
        counters_.get().rpcErrored(method);
}

void
RPCEngine::notifyForwarded(std::string const& method)
{
    if (validHandler(method))
        counters_.get().rpcForwarded(method);
}

}  // namespace RPC
