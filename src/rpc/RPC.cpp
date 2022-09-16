#include <etl/ETLSource.h>
#include <rpc/Handlers.h>
#include <rpc/RPCHelpers.h>
#include <webserver/HttpBase.h>
#include <webserver/WsBase.h>

#include <boost/asio/spawn.hpp>

#include <unordered_map>

namespace RPC {

Context::Context(
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
    std::string const& clientIp_)
    : Taggable(tagFactory_.make(std::cref(session_->tag())))
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
    BOOST_LOG_TRIVIAL(debug) << tag() << "new Context created";
}

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
    std::string const& clientIp)
{
    boost::json::value commandValue = nullptr;
    if (!request.contains("command") && request.contains("method"))
        commandValue = request.at("method");
    else if (request.contains("command") && !request.contains("method"))
        commandValue = request.at("command");

    if (!commandValue.is_string())
        return {};

    std::string command = commandValue.as_string().c_str();

    return std::make_optional<Context>(
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
    RPC::Counters& counters,
    std::string const& clientIp)
{
    if (!request.contains("method") || !request.at("method").is_string())
        return {};

    std::string const& command = request.at("method").as_string().c_str();

    if (command == "subscribe" || command == "unsubscribe")
        return {};

    if (!request.at("params").is_array())
        return {};

    boost::json::array const& array = request.at("params").as_array();

    if (array.size() != 1)
        return {};

    if (!array.at(0).is_object())
        return {};

    return std::make_optional<Context>(
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

constexpr static WarningInfo warningInfos[]{
    {warnUNKNOWN, "Unknown warning"},
    {warnRPC_CLIO,
     "This is a clio server. clio only serves validated data. If you "
     "want to talk to rippled, include 'ledger_index':'current' in your "
     "request"},
    {warnRPC_OUTDATED, "This server may be out of date"},
    {warnRPC_RATE_LIMIT, "You are about to be rate limited"}};

WarningInfo const&
get_warning_info(warning_code code)
{
    for (WarningInfo const& info : warningInfos)
    {
        if (info.code == code)
        {
            return info;
        }
    }
    throw(std::out_of_range("Invalid warning_code"));
}

boost::json::object
make_warning(warning_code code)
{
    boost::json::object json;
    WarningInfo const& info(get_warning_info(code));
    json["id"] = code;
    json["message"] = static_cast<std::string>(info.message);
    return json;
}

boost::json::object
make_error(Error err)
{
    boost::json::object json;
    ripple::RPC::ErrorInfo const& info(ripple::RPC::get_error_info(err));
    json["error"] = info.token;
    json["error_code"] = static_cast<std::uint32_t>(err);
    json["error_message"] = info.message;
    json["status"] = "error";
    json["type"] = "response";
    return json;
}

boost::json::object
make_error(Status const& status)
{
    if (status.error == ripple::rpcUNKNOWN)
    {
        return {
            {"error", status.message},
            {"type", "response"},
            {"status", "error"}};
    }

    boost::json::object json;
    ripple::RPC::ErrorInfo const& info(
        ripple::RPC::get_error_info(status.error));
    json["error"] =
        status.strCode.size() ? status.strCode.c_str() : info.token.c_str();
    json["error_code"] = static_cast<std::uint32_t>(status.error);
    json["error_message"] =
        status.message.size() ? status.message.c_str() : info.message.c_str();
    json["status"] = "error";
    json["type"] = "response";
    return json;
}

using LimitRange = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>;
using HandlerFunction = std::function<Result(Context const&)>;

struct Handler
{
    std::string method;
    std::function<Result(Context const&)> handler;
    std::optional<LimitRange> limit;
    bool isClioOnly = false;
};

class HandlerTable
{
    std::unordered_map<std::string, Handler> handlerMap_;

public:
    HandlerTable(std::initializer_list<Handler> handlers)
    {
        for (auto const& handler : handlers)
        {
            handlerMap_[handler.method] = std::move(handler);
        }
    }

    bool
    contains(std::string const& method)
    {
        return handlerMap_.contains(method);
    }

    std::optional<LimitRange>
    getLimitRange(std::string const& command)
    {
        if (!handlerMap_.contains(command))
            return {};

        return handlerMap_[command].limit;
    }

    std::optional<HandlerFunction>
    getHandler(std::string const& command)
    {
        if (!handlerMap_.contains(command))
            return {};

        return handlerMap_[command].handler;
    }

    bool
    isClioOnly(std::string const& command)
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
    {"noripple_check", &doNoRippleCheck, {}},
    {"book_changes", &doBookChanges, {}},
    {"book_offers", &doBookOffers, LimitRange{1, 50, 100}},
    {"ledger", &doLedger, {}},
    {"ledger_data", &doLedgerData, LimitRange{1, 100, 2048}},
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

static std::unordered_set<std::string> forwardCommands{
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
validHandler(std::string const& method)
{
    return handlerTable.contains(method) || forwardCommands.contains(method);
}

bool
isClioOnly(std::string const& method)
{
    return handlerTable.isClioOnly(method);
}

Status
getLimit(RPC::Context const& context, std::uint32_t& limit)
{
    if (!handlerTable.getHandler(context.method))
        return Status{Error::rpcUNKNOWN_COMMAND};

    if (!handlerTable.getLimitRange(context.method))
        return Status{Error::rpcINVALID_PARAMS, "rpcDoesNotRequireLimit"};

    auto [lo, def, hi] = *handlerTable.getLimitRange(context.method);

    if (context.params.contains(JS(limit)))
    {
        if (!context.params.at(JS(limit)).is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = context.params.at(JS(limit)).as_int64();
        if (limit <= 0)
            return Status{Error::rpcINVALID_PARAMS, "limitNotPositive"};

        limit = std::clamp(limit, lo, hi);
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
            return Status{Error::rpcFAILED_TO_FORWARD};

        if (res->contains("result") && res->at("result").is_object())
            return res->at("result").as_object();

        return *res;
    }

    if (ctx.method == "ping")
        return boost::json::object{};

    if (ctx.backend->isTooBusy())
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__ << " Database is too busy. Rejecting request";
        return Status{Error::rpcTOO_BUSY};
    }

    auto method = handlerTable.getHandler(ctx.method);

    if (!method)
        return Status{Error::rpcUNKNOWN_COMMAND};

    try
    {
        BOOST_LOG_TRIVIAL(debug)
            << ctx.tag() << __func__ << " start executing rpc `" << ctx.method
            << '`';
        auto v = (*method)(ctx);
        BOOST_LOG_TRIVIAL(debug)
            << ctx.tag() << __func__ << " finish executing rpc `" << ctx.method
            << '`';

        if (auto object = std::get_if<boost::json::object>(&v))
            (*object)["validated"] = true;

        return v;
    }
    catch (InvalidParamsError const& err)
    {
        return Status{Error::rpcINVALID_PARAMS, err.what()};
    }
    catch (AccountNotFoundError const& err)
    {
        return Status{Error::rpcACT_NOT_FOUND, err.what()};
    }
    catch (Backend::DatabaseTimeout const& t)
    {
        BOOST_LOG_TRIVIAL(error) << __func__ << " Database timeout";
        return Status{Error::rpcTOO_BUSY};
    }
    catch (std::exception const& err)
    {
        BOOST_LOG_TRIVIAL(error)
            << ctx.tag() << __func__ << " caught exception : " << err.what();
        return Status{Error::rpcINTERNAL};
    }
}

}  // namespace RPC
