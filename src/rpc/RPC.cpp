#include <boost/asio/spawn.hpp>
#include <etl/ETLSource.h>
#include <rpc/Handlers.h>
#include <unordered_map>

namespace RPC {

std::optional<Context>
make_WsContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    std::shared_ptr<BackendInterface const> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptions,
    std::shared_ptr<ETLLoadBalancer> const& balancer,
    std::shared_ptr<WsBase> const& session,
    Backend::LedgerRange const& range,
    Counters& counters,
    std::string const& clientIp)
{
    if (!request.contains("command"))
        return {};

    std::string command = request.at("command").as_string().c_str();

    return Context{
        yc,
        command,
        1,
        request,
        backend,
        subscriptions,
        balancer,
        session,
        range,
        counters,
        clientIp};
}

std::optional<Context>
make_HttpContext(
    boost::asio::yield_context& yc,
    boost::json::object const& request,
    std::shared_ptr<BackendInterface const> const& backend,
    std::shared_ptr<SubscriptionManager> const& subscriptions,
    std::shared_ptr<ETLLoadBalancer> const& balancer,
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

    return Context{
        yc,
        command,
        1,
        array.at(0).as_object(),
        backend,
        subscriptions,
        balancer,
        nullptr,
        range,
        counters,
        clientIp};
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
static std::unordered_map<std::string, std::function<Result(Context const&)>>
    handlerTable{
        {"account_channels", &doAccountChannels},
        {"account_currencies", &doAccountCurrencies},
        {"account_info", &doAccountInfo},
        {"account_lines", &doAccountLines},
        {"account_objects", &doAccountObjects},
        {"account_offers", &doAccountOffers},
        {"account_tx", &doAccountTx},
        {"gateway_balances", &doGatewayBalances},
        {"noripple_check", &doNoRippleCheck},
        {"book_offers", &doBookOffers},
        {"channel_authorize", &doChannelAuthorize},
        {"channel_verify", &doChannelVerify},
        {"ledger", &doLedger},
        {"ledger_data", &doLedgerData},
        {"ledger_entry", &doLedgerEntry},
        {"ledger_range", &doLedgerRange},
        {"ledger_data", &doLedgerData},
        {"subscribe", &doSubscribe},
        {"server_info", &doServerInfo},
        {"unsubscribe", &doUnsubscribe},
        {"tx", &doTx},
        {"transaction_entry", &doTransactionEntry},
        {"random", &doRandom}};

static std::unordered_set<std::string> forwardCommands{
    "submit",
    "submit_multisigned",
    "fee",
    "path_find",
    "ripple_path_find",
    "manifest"};

bool
shouldForwardToRippled(Context const& ctx)
{
    auto request = ctx.params;

    if (forwardCommands.find(ctx.method) != forwardCommands.end())
        return true;

    if (request.contains("ledger_index"))
    {
        auto indexValue = request.at("ledger_index");
        if (indexValue.is_string())
        {
            std::string index = indexValue.as_string().c_str();
            return index == "current" || index == "closed";
        }
    }

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

    if (handlerTable.find(ctx.method) == handlerTable.end())
        return Status{Error::rpcUNKNOWN_COMMAND};

    auto method = handlerTable[ctx.method];

    try
    {
        return method(ctx);
    }
    catch (InvalidParamsError const& err)
    {
        return Status{Error::rpcINVALID_PARAMS, err.what()};
    }
    catch (AccountNotFoundError const& err)
    {
        return Status{Error::rpcACT_NOT_FOUND, err.what()};
    }
    catch (std::exception const& err)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__ << " caught exception : " << err.what();
        return Status{Error::rpcINTERNAL, err.what()};
    }
}
}  // namespace RPC
