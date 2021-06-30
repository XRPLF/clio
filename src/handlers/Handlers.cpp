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
    {"account_channels", std::bind(doAccountChannels, std::placeholders::_1)},
    {
        "account_currencies", 
        std::bind(doAccountCurrencies, std::placeholders::_1)
    },
    {"account_info", std::bind(doAccountInfo, std::placeholders::_1)},
    {"account_lines", std::bind(doAccountLines, std::placeholders::_1)},
    {"account_objects", std::bind(doAccountObjects, std::placeholders::_1)},
    {"account_offers", std::bind(doAccountOffers, std::placeholders::_1)},
    {"account_tx", std::bind(doAccountTx, std::placeholders::_1)},
    {"book_offers", std::bind(doBookOffers, std::placeholders::_1)},
    {"channel_authorize", std::bind(doChannelAuthorize, std::placeholders::_1)},
    {"channel_verify", std::bind(doChannelVerify, std::placeholders::_1)},
    {"ledger", std::bind(&doLedger, std::placeholders::_1)},
    {"ledger_data", std::bind(doLedgerData, std::placeholders::_1)},
    {"ledger_entry", std::bind(doLedgerEntry, std::placeholders::_1)},
    {"ledger_range", std::bind(doLedgerRange, std::placeholders::_1)},
    {"ledger_data", std::bind(doLedgerData, std::placeholders::_1)},
    {"subscribe", std::bind(doSubscribe, std::placeholders::_1)},
    {"unsubscribe", std::bind(doUnsubscribe, std::placeholders::_1)},
    {"tx", std::bind(doTx, std::placeholders::_1)},
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

    return method(ctx);
}

}