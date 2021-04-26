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
addLine(
    boost::json::array& jsonLines,
    ripple::SLE const& line,
    ripple::AccountID const& account,
    boost::optional<ripple::AccountID> const& peerAccount)
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

boost::json::object
doAccountLines(
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

    boost::optional<ripple::AccountID> peerAccount;
    if (request.contains("peer"))
    {
        if (!request.at("peer").is_string())
        {
            response["error"] = "peer should be a string";
            return response;
        }

        peerAccount = ripple::parseBase58<ripple::AccountID>(
            request.at("peer").as_string().c_str());
        if (!peerAccount)
        {
            response["error"] = "Invalid peer account";
            return response;
        }
    }

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
        

    response["lines"] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonLines = response.at("lines").as_array();

    auto const addToResponse = [&](ripple::SLE const& sle) {
        if (sle.getType() == ripple::ltRIPPLE_STATE)
        {
            if (limit-- == 0)
            {
                return false;
            }
            
            addLine(jsonLines, sle, accountID, peerAccount);
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