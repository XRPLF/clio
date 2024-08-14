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

#include "rpc/handlers/AccountNFTs.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Keylet.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/nft.h>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

namespace rpc {

AccountNFTsHandler::Result
AccountNFTsHandler::process(AccountNFTsHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    auto const accountID = accountFromStringStrict(input.account);
    auto const accountLedgerObject =
        sharedPtrBackend_->fetchLedgerObject(ripple::keylet::account(*accountID).key, lgrInfo.seq, ctx.yield);

    if (!accountLedgerObject)
        return Error{Status{RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    auto response = Output{};
    response.account = input.account;
    response.limit = input.limit;
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    // if a marker was passed, start at the page specified in marker. Else, start at the max page
    auto const pageKey =
        input.marker ? ripple::uint256{input.marker->c_str()} : ripple::keylet::nftpage_max(*accountID).key;
    auto const blob = sharedPtrBackend_->fetchLedgerObject(pageKey, lgrInfo.seq, ctx.yield);

    if (!blob)
        return Error{Status{RippledError::rpcINVALID_PARAMS, "invalidMarker"}};

    std::optional<ripple::SLE const> page{ripple::SLE{ripple::SerialIter{blob->data(), blob->size()}, pageKey}};
    auto numPages = 0u;

    while (page) {
        auto const arr = page->getFieldArray(ripple::sfNFTokens);

        for (auto const& nft : arr) {
            auto const nftokenID = nft[ripple::sfNFTokenID];

            response.nfts.push_back(toBoostJson(nft.getJson(ripple::JsonOptions::none)));
            auto& obj = response.nfts.back().as_object();

            // Pull out the components of the nft ID.
            obj[SFS(sfFlags)] = ripple::nft::getFlags(nftokenID);
            obj[SFS(sfIssuer)] = to_string(ripple::nft::getIssuer(nftokenID));
            obj[SFS(sfNFTokenTaxon)] = ripple::nft::toUInt32(ripple::nft::getTaxon(nftokenID));
            obj[JS(nft_serial)] = ripple::nft::getSerial(nftokenID);

            if (std::uint16_t const xferFee = {ripple::nft::getTransferFee(nftokenID)})
                obj[SFS(sfTransferFee)] = xferFee;
        }

        ++numPages;
        if (auto const npm = (*page)[~ripple::sfPreviousPageMin]) {
            auto const nextKey = ripple::Keylet(ripple::ltNFTOKEN_PAGE, *npm);
            if (numPages == input.limit) {
                response.marker = to_string(nextKey.key);
                return response;
            }

            auto const nextBlob = sharedPtrBackend_->fetchLedgerObject(nextKey.key, lgrInfo.seq, ctx.yield);
            page.emplace(ripple::SLE{ripple::SerialIter{nextBlob->data(), nextBlob->size()}, nextKey.key});
        } else {
            page.reset();
        }
    }

    return response;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountNFTsHandler::Output const& output)
{
    jv = {
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(account), output.account},
        {JS(account_nfts), output.nfts},
        {JS(limit), output.limit},
    };

    if (output.marker)
        jv.as_object()[JS(marker)] = *output.marker;
}

AccountNFTsHandler::Input
tag_invoke(boost::json::value_to_tag<AccountNFTsHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountNFTsHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = boost::json::value_to<std::string>(jsonObject.at(JS(account)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jsonObject.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jsonObject.at(JS(ledger_index))));
        }
    }

    if (jsonObject.contains(JS(limit)))
        input.limit = jsonObject.at(JS(limit)).as_int64();

    if (jsonObject.contains(JS(marker)))
        input.marker = boost::json::value_to<std::string>(jsonObject.at(JS(marker)));

    return input;
}

}  // namespace rpc
