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

// {
//   nft_id: <ident>
//   ledger_hash: <ledger>
//   ledger_index: <ledger_index>
// }

namespace clio::rpc {

std::variant<std::monostate, std::string, Status>
getURI(data::NFT const& dbResponse, Context const& context)
{
    // Fetch URI from ledger
    // The correct page will be > bookmark and <= last. We need to calculate
    // the first possible page however, since bookmark is not guaranteed to
    // exist.
    auto const bookmark = ripple::keylet::nftpage(
        ripple::keylet::nftpage_min(dbResponse.owner), dbResponse.tokenID);
    auto const last = ripple::keylet::nftpage_max(dbResponse.owner);

    ripple::uint256 nextKey = last.key;
    std::optional<ripple::STLedgerEntry> sle;

    // when this loop terminates, `sle` will contain the correct page for
    // this NFT.
    //
    // 1) We start at the last NFTokenPage, which is guaranteed to exist,
    // grab the object from the DB and deserialize it.
    //
    // 2) If that NFTokenPage has a PreviousPageMin value and the
    // PreviousPageMin value is > bookmark, restart loop. Otherwise
    // terminate and use the `sle` from this iteration.
    do
    {
        auto const blob = context.backend->fetchLedgerObject(
            ripple::Keylet(ripple::ltNFTOKEN_PAGE, nextKey).key,
            dbResponse.ledgerSequence,
            context.yield);

        if (!blob || blob->size() == 0)
            return Status{
                RippledError::rpcINTERNAL,
                "Cannot find NFTokenPage for this NFT"};

        sle = ripple::STLedgerEntry(
            ripple::SerialIter{blob->data(), blob->size()}, nextKey);

        if (sle->isFieldPresent(ripple::sfPreviousPageMin))
            nextKey = sle->getFieldH256(ripple::sfPreviousPageMin);

    } while (sle && sle->key() != nextKey && nextKey > bookmark.key);

    if (!sle)
        return Status{
            RippledError::rpcINTERNAL, "Cannot find NFTokenPage for this NFT"};

    auto const nfts = sle->getFieldArray(ripple::sfNFTokens);
    auto const nft = std::find_if(
        nfts.begin(),
        nfts.end(),
        [&dbResponse](ripple::STObject const& candidate) {
            return candidate.getFieldH256(ripple::sfNFTokenID) ==
                dbResponse.tokenID;
        });

    if (nft == nfts.end())
        return Status{
            RippledError::rpcINTERNAL, "Cannot find NFTokenPage for this NFT"};

    ripple::Blob const uriField = nft->getFieldVL(ripple::sfURI);

    // NOTE this cannot use a ternary or value_or because then the
    // expression's type is unclear. We want to explicitly set the `uri`
    // field to null when not present to avoid any confusion.
    if (std::string const uri = std::string(uriField.begin(), uriField.end());
        uri.size() > 0)
        return uri;
    return std::monostate{};
}

Result
doNFTInfo(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    auto const maybeTokenID = getNFTID(request);
    if (auto const status = std::get_if<Status>(&maybeTokenID); status)
        return *status;
    auto const tokenID = std::get<ripple::uint256>(maybeTokenID);

    auto const maybeLedgerInfo = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&maybeLedgerInfo); status)
        return *status;
    auto const lgrInfo = std::get<ripple::LedgerInfo>(maybeLedgerInfo);

    std::optional<data::NFT> dbResponse =
        context.backend->fetchNFT(tokenID, lgrInfo.seq, context.yield);
    if (!dbResponse)
        return Status{RippledError::rpcOBJECT_NOT_FOUND, "NFT not found"};

    response["nft_id"] = ripple::strHex(dbResponse->tokenID);
    response["ledger_index"] = dbResponse->ledgerSequence;
    response["owner"] = ripple::toBase58(dbResponse->owner);
    response["is_burned"] = dbResponse->isBurned;

    response["flags"] = ripple::nft::getFlags(dbResponse->tokenID);
    response["transfer_fee"] = ripple::nft::getTransferFee(dbResponse->tokenID);
    response["issuer"] =
        ripple::toBase58(ripple::nft::getIssuer(dbResponse->tokenID));
    response["nft_taxon"] =
        ripple::nft::toUInt32(ripple::nft::getTaxon(dbResponse->tokenID));
    response["nft_sequence"] = ripple::nft::getSerial(dbResponse->tokenID);

    if (!dbResponse->isBurned)
    {
        auto const maybeURI = getURI(*dbResponse, context);
        // An error occurred
        if (Status const* status = std::get_if<Status>(&maybeURI); status)
            return *status;
        // A URI was found
        if (std::string const* uri = std::get_if<std::string>(&maybeURI); uri)
            response["uri"] = *uri;
        // A URI was not found, explicitly set to null
        else
            response["uri"] = nullptr;
    }

    return response;
}

}  // namespace clio::rpc
