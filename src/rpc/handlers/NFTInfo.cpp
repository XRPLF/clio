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

#include "rpc/handlers/NFTInfo.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/nft.h>

#include <string>
#include <variant>

using namespace ripple;
using namespace ::rpc;

namespace rpc {

NFTInfoHandler::Result
NFTInfoHandler::process(NFTInfoHandler::Input input, Context const& ctx) const
{
    auto const tokenID = ripple::uint256{input.nftID.c_str()};
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<LedgerHeader>(lgrInfoOrStatus);
    auto const maybeNft = sharedPtrBackend_->fetchNFT(tokenID, lgrInfo.seq, ctx.yield);

    if (not maybeNft.has_value())
        return Error{Status{RippledError::rpcOBJECT_NOT_FOUND, "NFT not found"}};

    // TODO - this formatting is exactly the same and SHOULD REMAIN THE SAME
    // for each element of the `nfts_by_issuer` API. We should factor this out
    // so that the formats don't diverge. In the mean time, do not make any
    // changes to this formatting without making the same changes to that
    // formatting.
    auto const& nft = *maybeNft;
    auto output = NFTInfoHandler::Output{};

    output.nftID = strHex(nft.tokenID);
    output.ledgerIndex = nft.ledgerSequence;
    output.owner = toBase58(nft.owner);
    output.isBurned = nft.isBurned;
    output.flags = nft::getFlags(nft.tokenID);
    output.transferFee = nft::getTransferFee(nft.tokenID);
    output.issuer = toBase58(nft::getIssuer(nft.tokenID));
    output.taxon = nft::toUInt32(nft::getTaxon(nft.tokenID));
    output.serial = nft::getSerial(nft.tokenID);
    output.uri = strHex(nft.uri);

    return output;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, NFTInfoHandler::Output const& output)
{
    // TODO: use JStrings when they become available
    jv = boost::json::object{
        {JS(nft_id), output.nftID},
        {JS(ledger_index), output.ledgerIndex},
        {JS(owner), output.owner},
        {"is_burned", output.isBurned},
        {JS(flags), output.flags},
        {"transfer_fee", output.transferFee},
        {JS(issuer), output.issuer},
        {"nft_taxon", output.taxon},
        {JS(nft_serial), output.serial},
        {JS(validated), output.validated},
        {JS(uri), output.uri},
    };
}

NFTInfoHandler::Input
tag_invoke(boost::json::value_to_tag<NFTInfoHandler::Input>, boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    auto input = NFTInfoHandler::Input{};

    input.nftID = boost::json::value_to<std::string>(jsonObject.at(JS(nft_id)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jsonObject.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jsonObject.at(JS(ledger_index))));
        }
    }

    return input;
}

}  // namespace rpc
