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
#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/nftPageMask.h>
#include <boost/json.hpp>
#include <algorithm>
#include <rpc/RPCHelpers.h>

#include <backend/BackendInterface.h>
#include <backend/DBHelpers.h>

namespace clio::rpc {

std::unordered_map<std::string, ripple::LedgerEntryType> types{
    {"state", ripple::ltRIPPLE_STATE},
    {"ticket", ripple::ltTICKET},
    {"signer_list", ripple::ltSIGNER_LIST},
    {"payment_channel", ripple::ltPAYCHAN},
    {"offer", ripple::ltOFFER},
    {"escrow", ripple::ltESCROW},
    {"deposit_preauth", ripple::ltDEPOSIT_PREAUTH},
    {"check", ripple::ltCHECK},
    {"nft_page", ripple::ltNFTOKEN_PAGE},
    {"nft_offer", ripple::ltNFTOKEN_OFFER}};

Result
doAccountNFTs(Context const& context)
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

    if (!accountID)
        return Status{RippledError::rpcINVALID_PARAMS, "malformedAccount"};

    auto rawAcct = context.backend->fetchLedgerObject(
        ripple::keylet::account(accountID).key, lgrInfo.seq, context.yield);

    if (!rawAcct)
        return Status{RippledError::rpcACT_NOT_FOUND, "accountNotFound"};

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    ripple::uint256 marker;
    if (auto const status = getHexMarker(request, marker); status)
        return status;

    response[JS(account)] = ripple::toBase58(accountID);
    response[JS(validated)] = true;
    response[JS(limit)] = limit;

    std::uint32_t numPages = 0;
    response[JS(account_nfts)] = boost::json::value(boost::json::array_kind);
    auto& nfts = response.at(JS(account_nfts)).as_array();

    // if a marker was passed, start at the page specified in marker. Else,
    // start at the max page
    auto const pageKey =
        marker.isZero() ? ripple::keylet::nftpage_max(accountID).key : marker;

    auto const blob =
        context.backend->fetchLedgerObject(pageKey, lgrInfo.seq, context.yield);
    if (!blob)
        return response;
    std::optional<ripple::SLE const> page{
        ripple::SLE{ripple::SerialIter{blob->data(), blob->size()}, pageKey}};

    // Continue iteration from the current page
    while (page)
    {
        auto arr = page->getFieldArray(ripple::sfNFTokens);

        for (auto const& o : arr)
        {
            ripple::uint256 const nftokenID = o[ripple::sfNFTokenID];

            {
                nfts.push_back(
                    toBoostJson(o.getJson(ripple::JsonOptions::none)));
                auto& obj = nfts.back().as_object();

                // Pull out the components of the nft ID.
                obj[SFS(sfFlags)] = ripple::nft::getFlags(nftokenID);
                obj[SFS(sfIssuer)] =
                    to_string(ripple::nft::getIssuer(nftokenID));
                obj[SFS(sfNFTokenTaxon)] =
                    ripple::nft::toUInt32(ripple::nft::getTaxon(nftokenID));
                obj[JS(nft_serial)] = ripple::nft::getSerial(nftokenID);

                if (std::uint16_t xferFee = {
                        ripple::nft::getTransferFee(nftokenID)})
                    obj[SFS(sfTransferFee)] = xferFee;
            }
        }

        ++numPages;
        if (auto npm = (*page)[~ripple::sfPreviousPageMin])
        {
            auto const nextKey = ripple::Keylet(ripple::ltNFTOKEN_PAGE, *npm);
            if (numPages == limit)
            {
                response[JS(marker)] = to_string(nextKey.key);
                response[JS(limit)] = numPages;
                return response;
            }
            auto const nextBlob = context.backend->fetchLedgerObject(
                nextKey.key, lgrInfo.seq, context.yield);

            page.emplace(ripple::SLE{
                ripple::SerialIter{nextBlob->data(), nextBlob->size()},
                nextKey.key});
        }
        else
            page.reset();
    }

    return response;
}

Result
doAccountObjects(Context const& context)
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

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
        return status;

    std::optional<std::string> marker = {};
    if (request.contains("marker"))
    {
        if (!request.at("marker").is_string())
            return Status{RippledError::rpcINVALID_PARAMS, "markerNotString"};

        marker = request.at("marker").as_string().c_str();
    }

    std::optional<ripple::LedgerEntryType> objectType = {};
    if (request.contains(JS(type)))
    {
        if (!request.at(JS(type)).is_string())
            return Status{RippledError::rpcINVALID_PARAMS, "typeNotString"};

        std::string typeAsString = request.at(JS(type)).as_string().c_str();
        if (types.find(typeAsString) == types.end())
            return Status{RippledError::rpcINVALID_PARAMS, "typeInvalid"};

        objectType = types[typeAsString];
    }

    response[JS(account)] = ripple::to_string(accountID);
    response[JS(account_objects)] = boost::json::value(boost::json::array_kind);
    boost::json::array& jsonObjects =
        response.at(JS(account_objects)).as_array();

    auto const addToResponse = [&](ripple::SLE&& sle) {
        if (!objectType || objectType == sle.getType())
        {
            jsonObjects.push_back(toJson(sle));
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

    response[JS(ledger_hash)] = ripple::strHex(lgrInfo.hash);
    response[JS(ledger_index)] = lgrInfo.seq;

    if (auto status = std::get_if<rpc::Status>(&next))
        return *status;

    auto const& nextMarker = std::get<rpc::AccountCursor>(next);
    if (nextMarker.isNonZero())
        response[JS(marker)] = nextMarker.toString();

    return response;
}

}  // namespace clio::rpc
