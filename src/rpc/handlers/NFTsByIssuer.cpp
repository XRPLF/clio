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

#include <ripple/protocol/nft.h>
#include <rpc/RPCHelpers.h>
#include <rpc/handlers/NFTsByIssuer.h>

using namespace ripple;

namespace rpc {

NFTsByIssuerHandler::Result
NFTsByIssuerHandler::process(NFTsByIssuerHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );
    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<LedgerInfo>(lgrInfoOrStatus);

    auto const limit = input.limit.value_or(NFTsByIssuerHandler::LIMIT_DEFAULT);

    auto const issuer = accountFromStringStrict(input.issuer);
    auto const accountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(ripple::keylet::account(*issuer).key, lgrInfo.seq, ctx.yield);

    if (!accountLedgerObject)
        return Error{Status{RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    std::optional<uint256> cursor;
    if (input.marker)
        cursor = uint256{input.marker->c_str()};

    auto const dbResponse =
        sharedPtrBackend_->fetchNFTsByIssuer(*issuer, input.nftTaxon, lgrInfo.seq, limit, cursor, ctx.yield);

    auto output = NFTsByIssuerHandler::Output{};

    output.issuer = toBase58(*issuer);
    output.limit = limit;
    output.ledgerIndex = lgrInfo.seq;
    output.nftTaxon = input.nftTaxon;

    for (auto const& nft : dbResponse.nfts) {
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

        output.nfts.push_back(nftJson);
    }

    if (dbResponse.cursor.has_value())
        output.marker = strHex(*dbResponse.cursor);

    return output;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, NFTsByIssuerHandler::Output const& output)
{
    jv = {
        {JS(issuer), output.issuer},
        {JS(limit), output.limit},
        {JS(ledger_index), output.ledgerIndex},
        {"nfts", output.nfts},
        {JS(validated), output.validated},
    };

    if (output.marker.has_value())
        jv.as_object()[JS(marker)] = *(output.marker);

    if (output.nftTaxon.has_value())
        jv.as_object()["nft_taxon"] = *(output.nftTaxon);
}

NFTsByIssuerHandler::Input
tag_invoke(boost::json::value_to_tag<NFTsByIssuerHandler::Input>, boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    NFTsByIssuerHandler::Input input;

    input.issuer = jsonObject.at(JS(issuer)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jsonObject.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(jsonObject.at(JS(ledger_index)).as_string().c_str());
        }
    }

    if (jsonObject.contains(JS(limit)))
        input.limit = jsonObject.at(JS(limit)).as_int64();

    if (jsonObject.contains("nft_taxon"))
        input.nftTaxon = jsonObject.at("nft_taxon").as_int64();

    if (jsonObject.contains(JS(marker)))
        input.marker = jsonObject.at(JS(marker)).as_string().c_str();

    return input;
}
}  // namespace rpc
