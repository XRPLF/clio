#include <handlers/Handlers.h>
#include <handlers/methods/Account.h>
#include <handlers/methods/Channel.h>
#include <handlers/methods/Exchange.h>
#include <handlers/methods/Ledger.h>
#include <handlers/methods/Subscribe.h>
#include <handlers/methods/Transaction.h>
#include <etl/ETLSource.h>

namespace RPC
{

static std::unordered_map<std::string, std::function<Result(Context const&)>> 
    handlerTable 
{
    { "account_channels", &doAccountChannels },
    { "account_currencies", &doAccountCurrencies },
    { "account_info", &doAccountInfo },
    { "account_lines", &doAccountLines },
    { "account_objects", &doAccountObjects },
    { "account_offers", &doAccountOffers },
    { "account_tx", &doAccountTx },
    { "book_offers", &doBookOffers },
    { "channel_authorize", &doChannelAuthorize },
    { "channel_verify", &doChannelVerify },
    { "ledger", &doLedger },
    { "ledger_data", &doLedgerData },
    { "ledger_entry", &doLedgerEntry },
    { "ledger_range", &doLedgerRange },
    { "ledger_data", &doLedgerData },
    { "subscribe", &doSubscribe },
    { "unsubscribe", &doUnsubscribe },
    { "tx", &doTx },
};

static std::unordered_set<std::string> forwardCommands {
    "submit",
    "submit_multisigned",
    "fee",
    "path_find",
    "ripple_path_find",
    "manifest"
};

bool
shouldForwardToRippled(Context const& ctx)
{
    auto request = ctx.params;

    if (request.contains("forward") && request.at("forward").is_bool())
        return request.at("forward").as_bool();

    BOOST_LOG_TRIVIAL(debug) << "checked forward";

    if (forwardCommands.find(ctx.method) != forwardCommands.end())
        return true;

    BOOST_LOG_TRIVIAL(debug) << "checked command";

    if (request.contains("ledger_index"))
    {
        auto indexValue = request.at("ledger_index");
        if (indexValue.is_string())
        {
            BOOST_LOG_TRIVIAL(debug) << "checking ledger as string";
            std::string index = indexValue.as_string().c_str();
            return index == "current" || index == "closed";
        }
    }
    
    BOOST_LOG_TRIVIAL(debug) << "checked ledger";

    return false;
}

Result
buildResponse(Context const& ctx)
{
    if (shouldForwardToRippled(ctx))
        return ctx.balancer->forwardToRippled(ctx.params);

    if (handlerTable.find(ctx.method) == handlerTable.end())
        return Status{Error::rpcUNKNOWN_COMMAND};

    auto method = handlerTable[ctx.method];

    return method(std::cref(ctx));
}

}