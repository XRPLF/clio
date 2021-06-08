#include <ripple/app/ledger/Ledger.h>
#include <ripple/app/paths/RippleState.h>
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

std::unordered_map<std::string, ripple::LedgerEntryType> types{
    {"state", ripple::ltRIPPLE_STATE},
    {"ticket", ripple::ltTICKET},
    {"signer_list", ripple::ltSIGNER_LIST},
    {"payment_channel", ripple::ltPAYCHAN},
    {"offer", ripple::ltOFFER},
    {"escrow", ripple::ltESCROW},
    {"deposit_preauth", ripple::ltDEPOSIT_PREAUTH},
    {"check", ripple::ltCHECK},
};

boost::json::object
doAccountObjects(
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

    if (!request.contains("account"))
    {
        response["error"] = "Must contain account";
        return response;
    }

    if (!request.at("account").is_string())
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

    ripple::uint256 cursor = beast::zero;
    if (request.contains("cursor"))
    {
        if (!request.at("cursor").is_string())
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

    std::optional<ripple::LedgerEntryType> objectType = {};
    if (request.contains("type"))
    {
        if (!request.at("type").is_string())
        {
            response["error"] = "type must be string";
            return response;
        }

        std::string typeAsString = request.at("type").as_string().c_str();
        if (types.find(typeAsString) == types.end())
        {
            response["error"] = "invalid object type";
            return response;
        }

        objectType = types[typeAsString];
    }

    response["objects"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonObjects = response.at("objects").as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (!objectType || objectType == sle.getType())
        {
            jsonObjects.push_back(toJson(sle));
        }

        return true;
    };

    auto nextCursor = traverseOwnedNodes(
        backend, accountID, *ledgerSequence, cursor, addToResponse);

    if (nextCursor)
        response["next_cursor"] = ripple::strHex(*nextCursor);

    return response;
}
