#include <ripple/app/ledger/Ledger.h>
#include <ripple/app/paths/RippleState.h>
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

namespace RPC {

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

Result
doAccountObjects(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);

    if (!request.contains("account"))
        return Status{Error::rpcINVALID_PARAMS, "missingAccount"};

    if (!request.at("account").is_string())
        return Status{Error::rpcINVALID_PARAMS, "accountNotString"};

    auto accountID =
        accountFromStringStrict(request.at("account").as_string().c_str());

    if (!accountID)
        return Status{Error::rpcINVALID_PARAMS, "malformedAccount"};

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if (!request.at("limit").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return Status{Error::rpcINVALID_PARAMS, "limitNotPositive"};
    }

    std::optional<std::string> cursor = {};
    if (request.contains("marker"))
    {
        if (!request.at("marker").is_string())
            return Status{Error::rpcINVALID_PARAMS, "markerNotString"};

        cursor = request.at("marker").as_string().c_str();
    }

    std::optional<ripple::LedgerEntryType> objectType = {};
    if (request.contains("type"))
    {
        if (!request.at("type").is_string())
            return Status{Error::rpcINVALID_PARAMS, "typeNotString"};

        std::string typeAsString = request.at("type").as_string().c_str();
        if (types.find(typeAsString) == types.end())
            return Status{Error::rpcINVALID_PARAMS, "typeInvalid"};

        objectType = types[typeAsString];
    }

    response["account"] = ripple::to_string(*accountID);
    response["account_objects"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonObjects = response.at("account_objects").as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (!objectType || objectType == sle.getType())
        {
            jsonObjects.push_back(toJson(sle));
        }
    };

    auto next = traverseOwnedNodes(
        *context.backend,
        *accountID,
        lgrInfo.seq,
        limit,
        cursor,
        context.yield,
        addToResponse);

    response["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    response["ledger_index"] = lgrInfo.seq;

    if (auto status = std::get_if<RPC::Status>(&next))
        return *status;

    auto nextCursor = std::get<RPC::AccountCursor>(next);

    if (nextCursor.isNonZero())
        response["marker"] = nextCursor.toString();

    return response;
}

}  // namespace RPC
