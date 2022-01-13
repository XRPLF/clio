#include <ripple/app/paths/RippleState.h>
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


namespace RPC
{

void
addLine(
    boost::json::array& jsonLines,
    ripple::SLE const& line,
    ripple::AccountID const& account,
    std::optional<ripple::AccountID> const& peerAccount)
{
    auto flags = line.getFieldU32(ripple::sfFlags);
    auto lowLimit = line.getFieldAmount(ripple::sfLowLimit);
    auto highLimit = line.getFieldAmount(ripple::sfHighLimit);
    auto lowID = lowLimit.getIssuer();
    auto highID = highLimit.getIssuer();
    auto lowQualityIn = line.getFieldU32(ripple::sfLowQualityIn);
    auto lowQualityOut = line.getFieldU32(ripple::sfLowQualityOut);
    auto highQualityIn = line.getFieldU32(ripple::sfHighQualityIn);
    auto highQualityOut = line.getFieldU32(ripple::sfHighQualityOut);
    auto balance = line.getFieldAmount(ripple::sfBalance);

    bool viewLowest = (lowID == account);
    auto lineLimit = viewLowest ? lowLimit : highLimit;
    auto lineLimitPeer = !viewLowest ? lowLimit : highLimit;
    auto lineAccountIDPeer = !viewLowest ? lowID : highID;
    auto lineQualityIn = viewLowest ? lowQualityIn : highQualityIn;
    auto lineQualityOut = viewLowest ? lowQualityOut : highQualityOut;

    if (peerAccount and peerAccount != lineAccountIDPeer)
        return;

    if (!viewLowest)
        balance.negate();

    bool lineAuth = flags & (viewLowest ? ripple::lsfLowAuth : ripple::lsfHighAuth);
    bool lineAuthPeer = flags & (!viewLowest ? ripple::lsfLowAuth : ripple::lsfHighAuth);
    bool lineNoRipple = flags & (viewLowest ? ripple::lsfLowNoRipple : ripple::lsfHighNoRipple);
    bool lineDefaultRipple = flags & ripple::lsfDefaultRipple;
    bool lineNoRipplePeer = flags & (!viewLowest ? ripple::lsfLowNoRipple : ripple::lsfHighNoRipple);
    bool lineFreeze = flags & (viewLowest ? ripple::lsfLowFreeze : ripple::lsfHighFreeze);
    bool lineFreezePeer = flags & (!viewLowest ? ripple::lsfLowFreeze : ripple::lsfHighFreeze);

    ripple::STAmount const& saBalance(balance);
    ripple::STAmount const& saLimit(lineLimit);
    ripple::STAmount const& saLimitPeer(lineLimitPeer);

    boost::json::object jPeer;
    jPeer["account"] = ripple::to_string(lineAccountIDPeer);
    jPeer["balance"] = saBalance.getText();
    jPeer["currency"] = ripple::to_string(saBalance.issue().currency);
    jPeer["limit"] = saLimit.getText();
    jPeer["limit_peer"] = saLimitPeer.getText();
    jPeer["quality_in"] = lineQualityIn;
    jPeer["quality_out"] = lineQualityOut;
    if (lineAuth)
        jPeer["authorized"] = true;
    if (lineAuthPeer)
        jPeer["peer_authorized"] = true;
    if (lineNoRipple || !lineDefaultRipple)
        jPeer["no_ripple"] = lineNoRipple;
    if (lineNoRipple || !lineDefaultRipple)
        jPeer["no_ripple_peer"] = lineNoRipplePeer;
    if (lineFreeze)
        jPeer["freeze"] = true;
    if (lineFreezePeer)
        jPeer["freeze_peer"] = true;

    jsonLines.push_back(jPeer);
}

Result 
doAccountLines(Context const& context)
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

    std::optional<ripple::AccountID> peerAccount;
    if (request.contains("peer"))
    {
        if (!request.at("peer").is_string())
            return Status{Error::rpcINVALID_PARAMS, "peerNotString"};

        peerAccount = accountFromStringStrict(
            request.at("peer").as_string().c_str());

        if (!peerAccount)
            return Status{Error::rpcINVALID_PARAMS, "peerMalformed"};
    }

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
    response["lines"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonLines = response.at("lines").as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltRIPPLE_STATE)
        {
            if (limit-- == 0)
            {
                return false;
            }
            
            addLine(jsonLines, sle, *accountID, peerAccount);
        }

        return true;
    };

    auto nextCursor = 
        traverseOwnedNodes(
            *context.backend,
            *accountID,
            lgrInfo.seq,
            cursor,
            context.yield,
            addToResponse);

    if (nextCursor)
        response["marker"] = ripple::strHex(*nextCursor);

    return response;
}

} // namespace RPC