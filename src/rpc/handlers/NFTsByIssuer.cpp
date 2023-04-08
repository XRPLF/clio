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

    auto const dbResponse = context.backend->fetchNFTIDsByIssuer(issuer, maybeTaxon, limit, maybeCursor, context.yield);

    boost::json::object response = {};
    response["issuer"] = ripple::toBase58(issuer);
    if (maybeTaxon.has_value())
        response["taxon"] = *maybeTaxon;

    boost::json::array nfts;

    // TODO try std::execution::par_unseq with transform and then remove_if?
    for (auto const nftID : dbResponse)
    {
        auto const nftResponse = context.backend->fetchNFT(nftID, lgrInfo.seq, context.yield);
        if (!nftResponse)
            continue;

        // TODO - this formatting is exactly the same and SHOULD REMAIN THE SAME
        // as the `nft_info` API. We should factor this out
        // so that the formats don't diverge. In the mean time, do not make any
        // changes to this formatting without making the same changes to that
        // formatting.
        boost::json::object nft = {};

        nft[JS(nft_id)] = ripple::strHex(nftResponse->tokenID);
        nft[JS(ledger_index)] = nftResponse->ledgerSequence;
        nft[JS(owner)] = ripple::toBase58(nftResponse->owner);
        nft["is_burned"] = nftResponse->isBurned;
        nft[JS(uri)] = ripple::strHex(nftResponse->uri);

        nft[JS(flags)] = ripple::nft::getFlags(nftResponse->tokenID);
        nft["transfer_fee"] = ripple::nft::getTransferFee(nftResponse->tokenID);
        nft[JS(issuer)] = ripple::toBase58(ripple::nft::getIssuer(nftResponse->tokenID));
        nft["nft_taxon"] = ripple::nft::toUInt32(ripple::nft::getTaxon(nftResponse->tokenID));
        nft[JS(nft_serial)] = ripple::nft::getSerial(nftResponse->tokenID);

        nfts.push_back(nft);
    }

    response["nfts"] = nfts;

    if (dbResponse.size() >= limit)
        response["marker"] = ripple::strHex(dbResponse.back());

    return response;
}

}  // namespace RPC
