#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <handlers/RPCHelpers.h>
#include <reporting/BackendInterface.h>
#include <reporting/DBHelpers.h>
#include <reporting/Pg.h>

boost::json::object
doAccountCurrencies(
    boost::json::object const& request,
    BackendInterface const& backend)
{
    boost::json::object response;

    auto ledgerSequence = ledgerSequenceFromRequest(request, backend);
    if (!ledgerSequence)
    {
        response["error"] = "Empty database";
        return response;
    }

    if(!request.contains("account"))
    {
        response["error"] = "Must contain account";
        return response;
    }

    if(!request.at("account").is_string())
    {
        response["error"] = "Account must be a string";
        return response;
    }
    
    ripple::AccountID accountID;
    auto parsed = ripple::parseBase58<ripple::AccountID>(
            request.at("account").as_string().c_str());

    if (!parsed)
    {
        response["error"] = "Invalid account";
        return response;
    }

    accountID = *parsed;


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
        backend,
        accountID,
        *ledgerSequence,
        beast::zero,
        addToResponse);

    response["send_currencies"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonSend = response.at("send_currencies").as_array();

    for (auto const& currency : send)
        jsonSend.push_back(currency.c_str());

    response["receive_currencies"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonReceive = response.at("receive_currencies").as_array();

    for (auto const& currency : receive)
        jsonReceive.push_back(currency.c_str());

    return response;
}
