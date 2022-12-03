//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

    Permission to use, copy, modify, and distribute this software for any 
    purpose with or without fee is hereby granted, provided that the above 
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <ripple/app/ledger/Ledger.h>
#include <ripple/app/paths/TrustLine.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <boost/json.hpp>

#include <algorithm>
#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

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

    if (peerAccount && peerAccount != lineAccountIDPeer)
        return;

    if (!viewLowest)
        balance.negate();

    bool lineAuth =
        flags & (viewLowest ? ripple::lsfLowAuth : ripple::lsfHighAuth);
    bool lineAuthPeer =
        flags & (!viewLowest ? ripple::lsfLowAuth : ripple::lsfHighAuth);
    bool lineNoRipple =
        flags & (viewLowest ? ripple::lsfLowNoRipple : ripple::lsfHighNoRipple);
    bool lineDefaultRipple = flags & ripple::lsfDefaultRipple;
    bool lineNoRipplePeer = flags &
        (!viewLowest ? ripple::lsfLowNoRipple : ripple::lsfHighNoRipple);
    bool lineFreeze =
        flags & (viewLowest ? ripple::lsfLowFreeze : ripple::lsfHighFreeze);
    bool lineFreezePeer =
        flags & (!viewLowest ? ripple::lsfLowFreeze : ripple::lsfHighFreeze);

    ripple::STAmount const& saBalance(balance);
    ripple::STAmount const& saLimit(lineLimit);
    ripple::STAmount const& saLimitPeer(lineLimitPeer);

    boost::json::object jPeer;
    jPeer[JS(account)] = ripple::to_string(lineAccountIDPeer);
    jPeer[JS(balance)] = saBalance.getText();
    jPeer[JS(currency)] = ripple::to_string(saBalance.issue().currency);
    jPeer[JS(limit)] = saLimit.getText();
    jPeer[JS(limit_peer)] = saLimitPeer.getText();
    jPeer[JS(quality_in)] = lineQualityIn;
    jPeer[JS(quality_out)] = lineQualityOut;
    if (lineAuth)
        jPeer[JS(authorized)] = true;
    if (lineAuthPeer)
        jPeer[JS(peer_authorized)] = true;
    if (lineNoRipple || !lineDefaultRipple)
        jPeer[JS(no_ripple)] = lineNoRipple;
    if (lineNoRipple || !lineDefaultRipple)
        jPeer[JS(no_ripple_peer)] = lineNoRipplePeer;
    if (lineFreeze)
        jPeer[JS(freeze)] = true;
    if (lineFreezePeer)
        jPeer[JS(freeze_peer)] = true;

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

    ripple::AccountID accountID;
    if (auto const status = getAccount(request, accountID); status)
        return status;

    auto rawAcct = context.backend->fetchLedgerObject(
        ripple::keylet::account(accountID).key, lgrInfo.seq, context.yield);

    if (!rawAcct)
        return Status{RippledError::rpcACT_NOT_FOUND, "accountNotFound"};

    std::optional<ripple::AccountID> peerAccount;
    if (auto const status = getOptionalAccount(request, peerAccount, JS(peer));
        status)
        return status;

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    std::optional<std::string> marker = {};
    if (request.contains(JS(marker)))
    {
        if (not request.at(JS(marker)).is_string())
            return Status{RippledError::rpcINVALID_PARAMS, "markerNotString"};

        marker = request.at(JS(marker)).as_string().c_str();
    }

    auto ignoreDefault = false;
    if (request.contains(JS(ignore_default)))
    {
        if (not request.at(JS(ignore_default)).is_bool())
            return Status{
                RippledError::rpcINVALID_PARAMS, "ignoreDefaultNotBool"};

        ignoreDefault = request.at(JS(ignore_default)).as_bool();
    }

    response[JS(account)] = ripple::to_string(accountID);
    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    response[JS(ledger_index)] = lgrInfo.seq;
    response[JS(limit)] = limit;
    response[JS(lines)] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonLines = response.at(JS(lines)).as_array();

    auto const addToResponse = [&](ripple::SLE&& sle) -> void {
        if (sle.getType() == ripple::ltRIPPLE_STATE)
        {
            auto ignore = false;
            if (ignoreDefault)
            {
                if (sle.getFieldAmount(ripple::sfLowLimit).getIssuer() ==
                    accountID)
                    ignore =
                        !(sle.getFieldU32(ripple::sfFlags) &
                          ripple::lsfLowReserve);
                else
                    ignore =
                        !(sle.getFieldU32(ripple::sfFlags) &
                          ripple::lsfHighReserve);
            }

            if (!ignore)
                addLine(jsonLines, sle, accountID, peerAccount);
        }
    };

    auto next = traverseOwnedNodes(
        *context.backend,
        accountID,
        lgrInfo.seq,
        limit,
        marker,
        context.yield,
        addToResponse);

    if (auto status = std::get_if<RPC::Status>(&next))
        return *status;

    auto nextMarker = std::get<RPC::AccountCursor>(next);

    if (nextMarker.isNonZero())
        response[JS(marker)] = nextMarker.toString();

    return response;
}

}  // namespace RPC
