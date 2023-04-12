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

#include <rpc/RPCHelpers.h>
#include <rpc/ngHandlers/NFTsByIssuer.h>

#include <ripple/app/tx/impl/details/NFTokenUtils.h>

using namespace ripple;
using namespace ::RPC;

namespace RPCng {

NFTsByIssuerHandler::Result
NFTsByIssuerHandler::process(NFTsByIssuerHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence);
    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<LedgerInfo>(lgrInfoOrStatus);

    auto const limit = input.limit.value_or(NFTsByIssuerHandler::LIMIT_DEFAULT);

    auto const issuer = accountFromStringStrict(input.nftIssuer);

    std::optional<uint256> cursor;
    if (input.marker)
        cursor = uint256{input.marker->c_str()};

    auto const dbResponse =
        sharedPtrBackend_->fetchNFTsByIssuer(*issuer, input.nftTaxon, lgrInfo.seq, limit, cursor, ctx.yield);

    auto output = NFTsByIssuerHandler::Output{};

    output.nftIssuer = toBase58(*issuer);
    output.limit = limit;
    output.ledgerIndex = lgrInfo.seq;
    output.nftTaxon = input.nftTaxon;

    std::transform(dbResponse.nfts.begin(), dbResponse.nfts.end(), output.nfts.begin(), [](auto const& nft) {
        // TODO - this formatting is exactly the same and SHOULD REMAIN THE SAME
        // for each element of the `nfts_by_issuer` API. We should factor this out
        // so that the formats don't diverge. In the mean time, do not make any
        // changes to this formatting without making the same changes to that
        // formatting.
        boost::json::object nftJson;

        nftJson[JS(nft_id)] = strHex(nft.tokenID);
        nftJson[JS(ledger_index)] = nft.ledgerSequence;
        nftJson[JS(owner)] = toBase58(nft.owner);
        nftJson["is_burned"] = nft.isBurned;
        nftJson[JS(uri)] = strHex(nft.uri);

        nftJson[JS(flags)] = nft::getFlags(nft.tokenID);
        nftJson["transfer_fee"] = nft::getTransferFee(nft.tokenID);
        nftJson[JS(issuer)] = toBase58(nft::getIssuer(nft.tokenID));
        nftJson["nft_taxon"] = nft::toUInt32(nft::getTaxon(nft.tokenID));
        nftJson[JS(nft_serial)] = nft::getSerial(nft.tokenID);

        return nftJson;
    });

    if (dbResponse.cursor.has_value())
        output.marker = strHex(*dbResponse.cursor);

    return output;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, NFTsByIssuerHandler::Output const& output)
{
    auto object = boost::json::object{
        {"nft_issuer", output.nftIssuer},
        {JS(limit), output.limit},
        {JS(ledger_index), output.ledgerIndex},
        {"nfts", output.nfts},
        {JS(validated), output.validated},
    };

    if (output.marker.has_value())
        object[JS(marker)] = *output.marker;

    if (output.nftTaxon.has_value())
        object["nft_taxon"] = *output.nftTaxon;

    jv = std::move(object);
}

NFTsByIssuerHandler::Input
tag_invoke(boost::json::value_to_tag<NFTsByIssuerHandler::Input>, boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    NFTsByIssuerHandler::Input input;

    input.nftIssuer = jsonObject.at("nft_issuer").as_string().c_str();

    if (jsonObject.contains("nft_taxon"))
        input.nftTaxon = jsonObject.at("nft_taxon").as_int64();

    return input;
}
}  // namespace RPCng
