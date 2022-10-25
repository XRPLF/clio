#include <etl/ETLSource.h>
#include <log/Logger.h>
#include <rpc/Handlers.h>
#include <rpc/RPCHelpers.h>
#include <webserver/HttpBase.h>
#include <webserver/WsBase.h>

#include <boost/asio/spawn.hpp>

#include <unordered_map>

using namespace std;
using namespace clio;

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
    shared_ptr<BackendInterface const> const& backend_,
    shared_ptr<SubscriptionManager> const& subscriptions_,
    shared_ptr<ETLLoadBalancer> const& balancer_,
    shared_ptr<ReportingETL const> const& etl_,
    shared_ptr<WsBase> const& session_,
    util::TagDecoratorFactory const& tagFactory_,
    Backend::LedgerRange const& range_,
    Counters& counters_,
    string const& clientIp_)
    : Taggable(tagFactory_)
    , yield(yield_)
    , method(command_)
    , version(version_)
    , params(params_)
    , backend(backend_)
    , subscriptions(subscriptions_)
    , balancer(balancer_)
    , etl(etl_)
    , session(session_)
    , range(range_)
    , counters(counters_)
    , clientIp(clientIp_)
{
    gPerfLog.debug() << tag() << "new Context created";
}

optional<Context>
make_WsContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    shared_ptr<BackendInterface const> const& backend,
    shared_ptr<SubscriptionManager> const& subscriptions,
    shared_ptr<ETLLoadBalancer> const& balancer,
    shared_ptr<ReportingETL const> const& etl,
    shared_ptr<WsBase> const& session,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    Counters& counters,
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

    return make_optional<Context>(
        yc,
        command,
        1,
        request,
        backend,
        subscriptions,
        balancer,
        etl,
        session,
        tagFactory,
        range,
        counters,
        clientIp);
}

optional<Context>
make_HttpContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    shared_ptr<BackendInterface const> const& backend,
    shared_ptr<SubscriptionManager> const& subscriptions,
    shared_ptr<ETLLoadBalancer> const& balancer,
    shared_ptr<ReportingETL const> const& etl,
    util::TagDecoratorFactory const& tagFactory,
    Backend::LedgerRange const& range,
    RPC::Counters& counters,
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

    return make_optional<Context>(
        yc,
        command,
        1,
        array.at(0).as_object(),
        backend,
        subscriptions,
        balancer,
        etl,
        nullptr,
        tagFactory,
        range,
        counters,
        clientIp);
}

using LimitRange = tuple<uint32_t, uint32_t, uint32_t>;
using HandlerFunction = function<Result(Context const&)>;

struct Handler
{
    string method;
    function<Result(Context const&)> handler;
    optional<LimitRange> limit;
    bool isClioOnly = false;
};

class HandlerTable
{
    unordered_map<string, Handler> handlerMap_;

public:
    HandlerTable(initializer_list<Handler> handlers)
    {
        for (auto const& handler : handlers)
        {
            handlerMap_[handler.method] = move(handler);
        }
    }

    bool
    contains(string const& method)
    {
        return handlerMap_.contains(method);
    }

    optional<LimitRange>
    getLimitRange(string const& command)
    {
        if (!handlerMap_.contains(command))
            return {};

        return handlerMap_[command].limit;
    }

    optional<HandlerFunction>
    getHandler(string const& command)
    {
        if (!handlerMap_.contains(command))
            return {};

        return handlerMap_[command].handler;
    }

    bool
    isClioOnly(string const& command)
    {
        return handlerMap_.contains(command) && handlerMap_[command].isClioOnly;
    }
};

static HandlerTable handlerTable{
    {"account_channels", &doAccountChannels, LimitRange{10, 50, 256}},
    {"account_currencies", &doAccountCurrencies, {}},
    {"account_info", &doAccountInfo, {}},
    {"account_lines", &doAccountLines, LimitRange{10, 50, 256}},
    {"account_nfts", &doAccountNFTs, LimitRange{1, 5, 10}},
    {"account_objects", &doAccountObjects, LimitRange{10, 50, 256}},
    {"account_offers", &doAccountOffers, LimitRange{10, 50, 256}},
    {"account_tx", &doAccountTx, LimitRange{1, 50, 100}},
    {"gateway_balances", &doGatewayBalances, {}},
    {"noripple_check", &doNoRippleCheck, LimitRange{1, 300, 500}},
    {"book_changes", &doBookChanges, {}},
    {"book_offers", &doBookOffers, LimitRange{1, 50, 100}},
    {"ledger", &doLedger, {}},
    {"ledger_data", &doLedgerData, LimitRange{1, 100, 2048}},
    {"amm_info", &doAMMInfo, {}},
    {"nft_buy_offers", &doNFTBuyOffers, LimitRange{1, 50, 100}},
    {"nft_history", &doNFTHistory, LimitRange{1, 50, 100}, true},
    {"nft_info", &doNFTInfo, {}, true},
    {"nft_sell_offers", &doNFTSellOffers, LimitRange{1, 50, 100}},
    {"ledger_entry", &doLedgerEntry, {}},
    {"ledger_range", &doLedgerRange, {}},
    {"subscribe", &doSubscribe, {}},
    {"server_info", &doServerInfo, {}},
    {"unsubscribe", &doUnsubscribe, {}},
    {"tx", &doTx, {}},
    {"transaction_entry", &doTransactionEntry, {}},
    {"random", &doRandom, {}}};

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

bool
validHandler(string const& method)
{
    return handlerTable.contains(method) || forwardCommands.contains(method);
}

bool
isClioOnly(string const& method)
{
    return handlerTable.isClioOnly(method);
}

bool
shouldSuppressValidatedFlag(RPC::Context const& context)
{
    return boost::iequals(context.method, "subscribe") ||
        boost::iequals(context.method, "unsubscribe");
}

Status
getLimit(RPC::Context const& context, uint32_t& limit)
{
    if (!handlerTable.getHandler(context.method))
        return Status{RippledError::rpcUNKNOWN_COMMAND};

    if (!handlerTable.getLimitRange(context.method))
        return Status{
            RippledError::rpcINVALID_PARAMS, "rpcDoesNotRequireLimit"};

    auto [lo, def, hi] = *handlerTable.getLimitRange(context.method);

    if (context.params.contains(JS(limit)))
    {
        string errMsg = "Invalid field 'limit', not unsigned integer.";
        if (!context.params.at(JS(limit)).is_int64())
            return Status{RippledError::rpcINVALID_PARAMS, errMsg};

        int input = context.params.at(JS(limit)).as_int64();
        if (input <= 0)
            return Status{RippledError::rpcINVALID_PARAMS, errMsg};

        limit = clamp(static_cast<uint32_t>(input), lo, hi);
    }
    else
    {
        limit = def;
    }

    return {};
}

bool
shouldForwardToRippled(Context const& ctx)
{
    auto request = ctx.params;

    if (isClioOnly(ctx.method))
        return false;

    if (forwardCommands.find(ctx.method) != forwardCommands.end())
        return true;

    if (specifiesCurrentOrClosedLedger(request))
        return true;

    if (ctx.method == "account_info" && request.contains("queue") &&
        request.at("queue").as_bool())
        return true;

    return false;
}

Result
buildResponse(Context const& ctx)
{
    if (shouldForwardToRippled(ctx))
    {
        boost::json::object toForward = ctx.params;
        toForward["command"] = ctx.method;

        auto res =
            ctx.balancer->forwardToRippled(toForward, ctx.clientIp, ctx.yield);

        ctx.counters.rpcForwarded(ctx.method);

        if (!res)
            return Status{RippledError::rpcFAILED_TO_FORWARD};

        if (res->contains("result") && res->at("result").is_object())
            return res->at("result").as_object();

        return *res;
    }

    if (ctx.method == "ping")
        return boost::json::object{};

    if (ctx.backend->isTooBusy())
    {
        gLog.error() << "Database is too busy. Rejecting request";
        return Status{RippledError::rpcTOO_BUSY};
    }

    auto method = handlerTable.getHandler(ctx.method);

    if (!method)
        return Status{RippledError::rpcUNKNOWN_COMMAND};

    try
    {
        gPerfLog.debug() << ctx.tag() << " start executing rpc `" << ctx.method
                         << '`';
        auto v = (*method)(ctx);
        gPerfLog.debug() << ctx.tag() << " finish executing rpc `" << ctx.method
                         << '`';

        if (auto object = get_if<boost::json::object>(&v);
            object && not shouldSuppressValidatedFlag(ctx))
        {
            (*object)[JS(validated)] = true;
        }

        return v;
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

}  // namespace RPC
