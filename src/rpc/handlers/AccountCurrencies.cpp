#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>

#include <rpc/RPCHelpers.h>
#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>
#include <backend/Pg.h>

namespace RPC
{

Result
doAccountCurrencies(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    if(!request.contains("account"))
        return Status{Error::rpcINVALID_PARAMS, "missingAccount"};

    if(!request.at("account").is_string())
        return Status{Error::rpcINVALID_PARAMS, "accountNotString"};
    
    auto accountID = 
        accountFromStringStrict(request.at("account").as_string().c_str());

    if (!accountID)
        return Status{Error::rpcINVALID_PARAMS, "malformedAccount"};

    std::set<std::string> send, receive;
    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltRIPPLE_STATE)
        {
            ripple::STAmount const& balance =
                 sle.getFieldAmount(ripple::sfBalance);

            auto lowLimit = sle.getFieldAmount(ripple::sfLowLimit);
            auto highLimit = sle.getFieldAmount(ripple::sfHighLimit);
            bool viewLowest = (lowLimit.getIssuer() == accountID);
            auto lineLimit = viewLowest ? lowLimit : highLimit;
            auto lineLimitPeer = !viewLowest ? lowLimit : highLimit;

            if (balance < lineLimit)
                receive.insert(ripple::to_string(balance.getCurrency()));
            if ((-balance) < lineLimitPeer)
                send.insert(ripple::to_string(balance.getCurrency()));
        }
        
        return true;
    };

    traverseOwnedNodes(
        *context.backend,
        *accountID,
        lgrInfo.seq,
        beast::zero,
        addToResponse);

    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;

    response["receive_currencies"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonReceive = response.at("receive_currencies").as_array();

    for (auto const& currency : receive)
        jsonReceive.push_back(currency.c_str());

    response["send_currencies"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonSend = response.at("send_currencies").as_array();

    for (auto const& currency : send)
        jsonSend.push_back(currency.c_str());

    return response;
}

} // namespace RPC
