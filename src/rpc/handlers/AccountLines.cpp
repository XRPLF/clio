//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <rpc/RPC.h>
#include <rpc/handlers/AccountLines.h>

namespace RPC {

void
AccountLinesHandler::addLine(
    std::vector<LineResponse>& lines,
    ripple::SLE const& lineSle,
    ripple::AccountID const& account,
    std::optional<ripple::AccountID> const& peerAccount) const
{
    auto const flags = lineSle.getFieldU32(ripple::sfFlags);
    auto const lowLimit = lineSle.getFieldAmount(ripple::sfLowLimit);
    auto const highLimit = lineSle.getFieldAmount(ripple::sfHighLimit);
    auto const lowID = lowLimit.getIssuer();
    auto const highID = highLimit.getIssuer();
    auto const lowQualityIn = lineSle.getFieldU32(ripple::sfLowQualityIn);
    auto const lowQualityOut = lineSle.getFieldU32(ripple::sfLowQualityOut);
    auto const highQualityIn = lineSle.getFieldU32(ripple::sfHighQualityIn);
    auto const highQualityOut = lineSle.getFieldU32(ripple::sfHighQualityOut);
    auto balance = lineSle.getFieldAmount(ripple::sfBalance);

    auto const viewLowest = (lowID == account);
    auto const lineLimit = viewLowest ? lowLimit : highLimit;
    auto const lineLimitPeer = not viewLowest ? lowLimit : highLimit;
    auto const lineAccountIDPeer = not viewLowest ? lowID : highID;
    auto const lineQualityIn = viewLowest ? lowQualityIn : highQualityIn;
    auto const lineQualityOut = viewLowest ? lowQualityOut : highQualityOut;

    if (peerAccount && peerAccount != lineAccountIDPeer)
        return;

    if (not viewLowest)
        balance.negate();

    bool const lineAuth = flags & (viewLowest ? ripple::lsfLowAuth : ripple::lsfHighAuth);
    bool const lineAuthPeer = flags & (not viewLowest ? ripple::lsfLowAuth : ripple::lsfHighAuth);
    bool const lineNoRipple = flags & (viewLowest ? ripple::lsfLowNoRipple : ripple::lsfHighNoRipple);
    bool const lineNoRipplePeer = flags & (not viewLowest ? ripple::lsfLowNoRipple : ripple::lsfHighNoRipple);
    bool const lineFreeze = flags & (viewLowest ? ripple::lsfLowFreeze : ripple::lsfHighFreeze);
    bool const lineFreezePeer = flags & (not viewLowest ? ripple::lsfLowFreeze : ripple::lsfHighFreeze);

    ripple::STAmount const& saBalance = balance;
    ripple::STAmount const& saLimit = lineLimit;
    ripple::STAmount const& saLimitPeer = lineLimitPeer;

    LineResponse line;
    line.account = ripple::to_string(lineAccountIDPeer);
    line.balance = saBalance.getText();
    line.currency = ripple::to_string(saBalance.issue().currency);
    line.limit = saLimit.getText();
    line.limitPeer = saLimitPeer.getText();
    line.qualityIn = lineQualityIn;
    line.qualityOut = lineQualityOut;

    if (lineAuth)
        line.authorized = true;

    if (lineAuthPeer)
        line.peerAuthorized = true;

    if (lineFreeze)
        line.freeze = true;

    if (lineFreezePeer)
        line.freezePeer = true;

    line.noRipple = lineNoRipple;
    line.noRipplePeer = lineNoRipplePeer;
    lines.push_back(line);
}

AccountLinesHandler::Result
AccountLinesHandler::process(AccountLinesHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence);

    if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerInfo>(lgrInfoOrStatus);
    auto const accountID = accountFromStringStrict(input.account);
    auto const accountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(ripple::keylet::account(*accountID).key, lgrInfo.seq, ctx.yield);

    if (not accountLedgerObject)
        return Error{Status{RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    auto const peerAccountID = input.peer ? accountFromStringStrict(*(input.peer)) : std::optional<ripple::AccountID>{};

    Output response;
    response.lines.reserve(input.limit);

    auto const addToResponse = [&](ripple::SLE&& sle) {
        if (sle.getType() == ripple::ltRIPPLE_STATE)
        {
            auto ignore = false;
            if (input.ignoreDefault)
            {
                if (sle.getFieldAmount(ripple::sfLowLimit).getIssuer() == accountID)
                    ignore = !(sle.getFieldU32(ripple::sfFlags) & ripple::lsfLowReserve);
                else
                    ignore = !(sle.getFieldU32(ripple::sfFlags) & ripple::lsfHighReserve);
            }

            if (not ignore)
                addLine(response.lines, sle, *accountID, peerAccountID);
        }
    };

    auto const next = ngTraverseOwnedNodes(
        *sharedPtrBackend_, *accountID, lgrInfo.seq, input.limit, input.marker, ctx.yield, addToResponse);
    auto const nextMarker = std::get<AccountCursor>(next);

    response.account = input.account;
    response.limit = input.limit;  // not documented,
                                   // https://github.com/XRPLF/xrpl-dev-portal/issues/1838
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

AccountLinesHandler::Input
tag_invoke(boost::json::value_to_tag<AccountLinesHandler::Input>, boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    AccountLinesHandler::Input input;

    input.account = jv.at(JS(account)).as_string().c_str();
    if (jsonObject.contains(JS(limit)))
        input.limit = jv.at(JS(limit)).as_int64();

    if (jsonObject.contains(JS(marker)))
        input.marker = jv.at(JS(marker)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jv.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(peer)))
        input.peer = jv.at(JS(peer)).as_string().c_str();

    if (jsonObject.contains(JS(ignore_default)))
        input.ignoreDefault = jv.at(JS(ignore_default)).as_bool();

    if (jsonObject.contains(JS(ledger_index)))
    {
        if (!jsonObject.at(JS(ledger_index)).is_string())
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        else if (jsonObject.at(JS(ledger_index)).as_string() != "validated")
            input.ledgerIndex = std::stoi(jv.at(JS(ledger_index)).as_string().c_str());
    }

    return input;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountLinesHandler::Output const& output)
{
    auto obj = boost::json::object{
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(limit), output.limit},
        {JS(lines), output.lines},
    };

    if (output.marker)
        obj[JS(marker)] = output.marker.value();

    jv = std::move(obj);
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    [[maybe_unused]] AccountLinesHandler::LineResponse const& line)
{
    auto obj = boost::json::object{
        {JS(account), line.account},
        {JS(balance), line.balance},
        {JS(currency), line.currency},
        {JS(limit), line.limit},
        {JS(limit_peer), line.limitPeer},
        {JS(quality_in), line.qualityIn},
        {JS(quality_out), line.qualityOut},
    };

    obj[JS(no_ripple)] = line.noRipple;
    obj[JS(no_ripple_peer)] = line.noRipplePeer;

    if (line.authorized)
        obj[JS(authorized)] = *(line.authorized);

    if (line.peerAuthorized)
        obj[JS(peer_authorized)] = *(line.peerAuthorized);

    if (line.freeze)
        obj[JS(freeze)] = *(line.freeze);

    if (line.freezePeer)
        obj[JS(freeze_peer)] = *(line.freezePeer);

    jv = std::move(obj);
}

}  // namespace RPC
