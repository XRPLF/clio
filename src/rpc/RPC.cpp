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
#include <webserver/HttpBase.h>
#include <webserver/WsBase.h>

#include <rpc/ngHandlers/AccountChannels.h>
#include <rpc/ngHandlers/AccountCurrencies.h>
#include <rpc/ngHandlers/AccountLines.h>
#include <rpc/ngHandlers/AccountOffers.h>
#include <rpc/ngHandlers/AccountTx.h>
#include <rpc/ngHandlers/BookOffers.h>
#include <rpc/ngHandlers/GatewayBalances.h>
#include <rpc/ngHandlers/LedgerEntry.h>
#include <rpc/ngHandlers/LedgerRange.h>
#include <rpc/ngHandlers/NFTBuyOffers.h>
#include <rpc/ngHandlers/NFTInfo.h>
#include <rpc/ngHandlers/NFTSellOffers.h>
#include <rpc/ngHandlers/NoRippleCheck.h>
#include <rpc/ngHandlers/Ping.h>
#include <rpc/ngHandlers/TransactionEntry.h>
#include <rpc/ngHandlers/Tx.h>

#include <boost/asio/spawn.hpp>

#include <unordered_map>

using namespace std;
using namespace clio;
using namespace RPCng;

// local to compilation unit loggers
namespace {
clio::Logger gPerfLog{"Performance"};
clio::Logger gLog{"RPC"};
}  // namespace

namespace RPC {
Context::Context(
    boost::asio::yield_context& yield_,
    string const& command_,
    uint32_t version_,
    boost::json::object const& params_,
    shared_ptr<WsBase> const& session_,
    util::TagDecoratorFactory const& tagFactory_,
    Backend::LedgerRange const& range_,
    string const& clientIp_)
    : Taggable(tagFactory_)
    , yield(yield_)
    , method(command_)
    , version(version_)
    , params(params_)
    , session(session_)
    , range(range_)
    , clientIp(clientIp_)
{
    gPerfLog.debug() << tag() << "new Context created";
}

optional<Context>
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

    return make_optional<Context>(yc, command, 1, request, session, tagFactory, range, clientIp);
}

optional<Context>
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

    return make_optional<Context>(yc, command, 1, array.at(0).as_object(), nullptr, tagFactory, range, clientIp);
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

HandlerTable::HandlerTable(std::shared_ptr<BackendInterface> const& backend)
    : handlerMap_{
          {"account_channels", {AnyHandler{AccountChannelsHandler{backend}}}},
          {"account_currencies", {AnyHandler{AccountCurrenciesHandler{backend}}}},
          {"account_lines", {AnyHandler{AccountLinesHandler{backend}}}},
          {"account_offers", {AnyHandler{AccountOffersHandler{backend}}}},
          {"account_tx", {AnyHandler{AccountTxHandler{backend}}}},
          {"book_offers", {AnyHandler{BookOffersHandler{backend}}}},
          {"gateway_balances", {AnyHandler{GatewayBalancesHandler{backend}}}},
          {"ledger_entry", {AnyHandler{LedgerEntryHandler{backend}}}},
          {"ledger_range", {AnyHandler{LedgerRangeHandler{backend}}}},
          {"nft_buy_offers", {AnyHandler{NFTBuyOffersHandler{backend}}}},
          {"nft_sell_offers", {AnyHandler{NFTSellOffersHandler{backend}}}},
          {"nft_info", {AnyHandler{NFTInfoHandler{backend}}}},
          {"noripple_check", {AnyHandler{NoRippleCheckHandler{backend}}}},
          {"ping", {AnyHandler{PingHandler{}}}},
          {"transaction_entry", {AnyHandler{TransactionEntryHandler{backend}}}},
          {"tx", {AnyHandler{TxHandler{backend}}}},
      }
{
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
RPCEngine::shouldSuppressValidatedFlag(RPC::Context const& context) const
{
    return boost::iequals(context.method, "subscribe") || boost::iequals(context.method, "unsubscribe");
}

bool
RPCEngine::shouldForwardToRippled(Context const& ctx) const
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
RPCEngine::buildResponse(Context const& ctx)
{
    if (shouldForwardToRippled(ctx))
    {
        auto toForward = ctx.params;
        toForward["command"] = ctx.method;

        auto const res = balancer_->forwardToRippled(toForward, ctx.clientIp, ctx.yield);

        counters_.rpcForwarded(ctx.method);

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
        auto const v = (*method).process(ctx.params, ctx.yield);
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
    counters_.rpcComplete(method, duration);
}

void
RPCEngine::notifyErrored(std::string const& method)
{
    counters_.rpcErrored(method);
}

}  // namespace RPC
