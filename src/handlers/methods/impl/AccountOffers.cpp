#include <ripple/app/paths/RippleState.h>
#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>
#include <algorithm>
#include <handlers/RPCHelpers.h>
#include <handlers/methods/Account.h>
#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>


namespace RPC
{

void
addOffer(boost::json::array& offersJson, ripple::SLE const& offer)
{
    auto quality = getQuality(offer.getFieldH256(ripple::sfBookDirectory));
    ripple::STAmount rate = ripple::amountFromQuality(quality);

    ripple::STAmount takerPays = offer.getFieldAmount(ripple::sfTakerPays);
    ripple::STAmount takerGets = offer.getFieldAmount(ripple::sfTakerGets);
    
    boost::json::object obj;

    if (!takerPays.native())
    {
        obj["taker_pays"] = boost::json::value(boost::json::object_kind);
        boost::json::object& takerPaysJson = obj.at("taker_pays").as_object();

        takerPaysJson["value"] = takerPays.getText();
        takerPaysJson["currency"] = ripple::to_string(takerPays.getCurrency());
        takerPaysJson["issuer"] = ripple::to_string(takerPays.getIssuer());
    }
    else
    {
        obj["taker_pays"] = takerPays.getText();
    }

    if (!takerGets.native())
    {
        obj["taker_gets"] = boost::json::value(boost::json::object_kind);
        boost::json::object& takerGetsJson = obj.at("taker_gets").as_object();

        takerGetsJson["value"] = takerGets.getText();
        takerGetsJson["currency"] = ripple::to_string(takerGets.getCurrency());
        takerGetsJson["issuer"] = ripple::to_string(takerGets.getIssuer());
    }
    else
    {
        obj["taker_gets"] = takerGets.getText();
    }

    obj["seq"] = offer.getFieldU32(ripple::sfSequence);
    obj["flags"] = offer.getFieldU32(ripple::sfFlags);
    obj["quality"] = rate.getText();
    if (offer.isFieldPresent(ripple::sfExpiration))
        obj["expiration"] = offer.getFieldU32(ripple::sfExpiration);
    
    offersJson.push_back(obj);
};

Result
doAccountOffers(Context const& context)
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


    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if(!request.at("limit").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return Status{Error::rpcINVALID_PARAMS, "limitNotPositive"};
    }

    ripple::uint256 cursor;
    if (request.contains("cursor"))
    {
        if(!request.at("cursor").is_string())
            return Status{Error::rpcINVALID_PARAMS, "cursorNotString"};

        if (!cursor.parseHex(request.at("cursor").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "malformedCursor"};
    }
    
    response["account"] = ripple::to_string(*accountID);
    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;
    response["offers"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonLines = response.at("offers").as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltOFFER)
        {
            if (limit-- == 0)
            {
                return false;
            }
            
            addOffer(jsonLines, sle);
        }

        return true;
    };
    
    auto nextCursor = 
        traverseOwnedNodes(
            *context.backend,
            *accountID,
            lgrInfo.seq,
            cursor,
            addToResponse);

    if (nextCursor)
        response["marker"] = ripple::strHex(*nextCursor);

    return OK;
}

} // namespace RPC