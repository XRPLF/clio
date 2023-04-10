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

#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <boost/json.hpp>

#include <rpc/RPCHelpers.h>

namespace RPC {

Result
doNFTsByIssuer(Context const& context)
{
    auto const request = context.params;

    ripple::AccountID issuer;
    if (auto const status = getAccount(request, issuer, "issuer"); status)
        return status;

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    auto const maybeLedgerInfo = ledgerInfoFromRequest(context);
    if (auto const status = std::get_if<Status>(&maybeLedgerInfo); status)
        return *status;
    auto const lgrInfo = std::get<ripple::LedgerInfo>(maybeLedgerInfo);

    auto const maybeTaxon = getUInt(request, "taxon");

    std::optional<ripple::uint256> maybeCursor;
    if (auto const maybeCursorStr = getString(request, "marker"); maybeCursorStr.has_value())
    {
        // TODO why is this necessary?
        maybeCursor = ripple::uint256{};
        if (!maybeCursor->parseHex(*maybeCursorStr))
            return Status{RippledError::rpcINVALID_PARAMS, "markerMalformed"};
    }

    auto const dbResponse =
        context.backend->fetchNFTsByIssuer(issuer, maybeTaxon, lgrInfo.seq, limit, maybeCursor, context.yield);

    boost::json::object response = {};

    response["issuer"] = ripple::toBase58(issuer);
    response["limit"] = limit;
    response["ledger_index"] = lgrInfo.seq;

    boost::json::array nftsJson;
    for (auto const& nft : dbResponse.nfts)
    {
        // TODO - this formatting is exactly the same and SHOULD REMAIN THE SAME
        // for each element of the `nfts_by_issuer` API. We should factor this out
        // so that the formats don't diverge. In the mean time, do not make any
        // changes to this formatting without making the same changes to that
        // formatting.
        boost::json::object nftJson;

        nftJson[JS(nft_id)] = ripple::strHex(nft.tokenID);
        nftJson[JS(ledger_index)] = nft.ledgerSequence;
        nftJson[JS(owner)] = ripple::toBase58(nft.owner);
        nftJson["is_burned"] = nft.isBurned;
        nftJson[JS(uri)] = ripple::strHex(nft.uri);

        nftJson[JS(flags)] = ripple::nft::getFlags(nft.tokenID);
        nftJson["transfer_fee"] = ripple::nft::getTransferFee(nft.tokenID);
        nftJson[JS(issuer)] = ripple::toBase58(ripple::nft::getIssuer(nft.tokenID));
        nftJson["nft_taxon"] = ripple::nft::toUInt32(ripple::nft::getTaxon(nft.tokenID));
        nftJson[JS(nft_serial)] = ripple::nft::getSerial(nft.tokenID);

        nftsJson.push_back(nftJson);
    }
    response["nfts"] = nftsJson;

    if (dbResponse.cursor.has_value())
        response["marker"] = ripple::strHex(*dbResponse.cursor);

    if (maybeTaxon.has_value())
        response["taxon"] = *maybeTaxon;

    return response;
}

}  // namespace RPC
