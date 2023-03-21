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

#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/protocol/Indexes.h>
#include <boost/json.hpp>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doNFTInfo(Context const& context)
{
    auto const request = context.params;
    boost::json::object response = {};

    auto const maybeTokenID = getNFTID(request);
    if (auto const status = std::get_if<Status>(&maybeTokenID); status)
        return *status;
    auto const tokenID = std::get<ripple::uint256>(maybeTokenID);

    auto const maybeLedgerInfo = ledgerInfoFromRequest(context);
    if (auto const status = std::get_if<Status>(&maybeLedgerInfo); status)
        return *status;
    auto const lgrInfo = std::get<ripple::LedgerInfo>(maybeLedgerInfo);

    auto const dbResponse =
        context.backend->fetchNFT(tokenID, lgrInfo.seq, context.yield);
    if (!dbResponse)
        return Status{RippledError::rpcOBJECT_NOT_FOUND, "NFT not found"};

    response[JS(nft_id)] = ripple::strHex(dbResponse->tokenID);
    response[JS(ledger_index)] = dbResponse->ledgerSequence;
    response[JS(owner)] = ripple::toBase58(dbResponse->owner);
    response["is_burned"] = dbResponse->isBurned;
    response[JS(uri)] = ripple::strHex(dbResponse->uri);

    response[JS(flags)] = ripple::nft::getFlags(dbResponse->tokenID);
    response["transfer_rate"] =
        ripple::nft::getTransferFee(dbResponse->tokenID);
    response[JS(issuer)] =
        ripple::toBase58(ripple::nft::getIssuer(dbResponse->tokenID));
    response["nft_taxon"] =
        ripple::nft::toUInt32(ripple::nft::getTaxon(dbResponse->tokenID));
    response[JS(nft_serial)] = ripple::nft::getSerial(dbResponse->tokenID);

    return response;
}

}  // namespace RPC
