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
#include <handlers/methods/Methods.h>
#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>


namespace RPC
{

std::unordered_map<std::string, ripple::LedgerEntryType> types {
    {"state", ripple::ltRIPPLE_STATE},
    {"ticket", ripple::ltTICKET},
    {"signer_list", ripple::ltSIGNER_LIST},
    {"payment_channel", ripple::ltPAYCHAN},
    {"offer", ripple::ltOFFER},
    {"escrow", ripple::ltESCROW},
    {"deposit_preauth", ripple::ltDEPOSIT_PREAUTH},
    {"check", ripple::ltCHECK},
};

Result
doAccountObjects(Context const& context)
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

    std::optional<ripple::LedgerEntryType> objectType = {};
    if (request.contains("type"))
    {
        if(!request.at("type").is_string())
            return Status{Error::rpcINVALID_PARAMS, "typeNotString"};

        std::string typeAsString = request.at("type").as_string().c_str();
        if(types.find(typeAsString) == types.end())
            return Status{Error::rpcINVALID_PARAMS, "typeInvalid"};

        objectType = types[typeAsString];
    }
    
    response["account"] = ripple::to_string(*accountID);
    response["account_objects"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonObjects = response.at("objects").as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (!objectType || objectType == sle.getType())
        {
            if (limit-- == 0)
            {
                return false;
            }

            jsonObjects.push_back(toJson(sle));
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

    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;

    if (nextCursor)
        response["marker"] = ripple::strHex(*nextCursor);

    return response;
}

} // namespace RPC