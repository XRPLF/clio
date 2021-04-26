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
#include <reporting/BackendInterface.h>
#include <reporting/DBHelpers.h>

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
boost::json::object
doAccountOffers(
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

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if(!request.at("limit").is_int64())
        {
            response["error"] = "limit must be integer";
            return response;
        }

        limit = request.at("limit").as_int64();
        if (limit <= 0)
        {
            response["error"] = "limit must be positive";
            return response;
        }
    }

    ripple::uint256 cursor = beast::zero;
    if (request.contains("cursor"))
    {
        if(!request.at("cursor").is_string())
        {
            response["error"] = "limit must be string";
            return response;
        }

        auto bytes = ripple::strUnHex(request.at("cursor").as_string().c_str());
        if (bytes and bytes->size() != 32)
        {
            response["error"] = "invalid cursor";
            return response;
        }

        cursor = ripple::uint256::fromVoid(bytes->data());
    }
        
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
            backend,
            accountID,
            *ledgerSequence,
            cursor,
            addToResponse);

    if (nextCursor)
        response["next_cursor"] = ripple::strHex(*nextCursor);

    return response;
}