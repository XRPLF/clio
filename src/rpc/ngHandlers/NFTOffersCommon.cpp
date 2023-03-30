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
#include <rpc/ngHandlers/NFTOffersCommon.h>

#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/protocol/Indexes.h>

using namespace ripple;
using namespace ::RPC;

namespace ripple {

// TODO: move to some common serialization impl place
inline void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    SLE const& offer)
{
    auto amount = ::RPC::toBoostJson(
        offer.getFieldAmount(sfAmount).getJson(JsonOptions::none));

    boost::json::object obj = {
        {JS(nft_offer_index), to_string(offer.key())},
        {JS(flags), offer[sfFlags]},
        {JS(owner), toBase58(offer.getAccountID(sfOwner))},
        {JS(amount), std::move(amount)},
    };

    if (offer.isFieldPresent(sfDestination))
        obj.insert_or_assign(
            JS(destination), toBase58(offer.getAccountID(sfDestination)));

    if (offer.isFieldPresent(sfExpiration))
        obj.insert_or_assign(JS(expiration), offer.getFieldU32(sfExpiration));

    jv = std::move(obj);
}

}  // namespace ripple

namespace RPCng {

NFTOffersHandlerBase::Result
NFTOffersHandlerBase::iterateOfferDirectory(
    Input input,
    ripple::uint256 const& tokenID,
    ripple::Keylet const& directory,
    boost::asio::yield_context& yield) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_,
        yield,
        input.ledgerHash,
        input.ledgerIndex,
        range->maxSequence);
    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<LedgerInfo>(lgrInfoOrStatus);

    // TODO: just check for existence without pulling
    if (not sharedPtrBackend_->fetchLedgerObject(
            directory.key, lgrInfo.seq, yield))
        return Error{Status{RippledError::rpcOBJECT_NOT_FOUND, "notFound"}};

    auto output = Output{input.nftID};
    auto offers = std::vector<ripple::SLE>{};
    auto reserve = input.limit;
    auto cursor = uint256{};
    auto startHint = uint64_t{0ul};

    if (input.marker)
    {
        cursor = uint256(input.marker->c_str());

        // We have a start point. Use limit - 1 from the result and use the
        // very last one for the resume.
        auto const sle =
            [this, &cursor, &lgrInfo, &yield]() -> std::shared_ptr<SLE const> {
            auto const key = keylet::nftoffer(cursor).key;
            if (auto const blob = sharedPtrBackend_->fetchLedgerObject(
                    key, lgrInfo.seq, yield);
                blob)
            {
                return std::make_shared<SLE const>(
                    SerialIter{blob->data(), blob->size()}, key);
            }
            return nullptr;
        }();

        if (!sle ||
            sle->getFieldU16(ripple::sfLedgerEntryType) !=
                ripple::ltNFTOKEN_OFFER ||
            tokenID != sle->getFieldH256(ripple::sfNFTokenID))
        {
            return Error{Status{RippledError::rpcINVALID_PARAMS}};
        }

        startHint = sle->getFieldU64(ripple::sfNFTokenOfferNode);
        output.offers.push_back(*sle);
        offers.reserve(reserve);
    }
    else
    {
        // We have no start point, limit should be one higher than
        // requested.
        offers.reserve(++reserve);
    }

    auto result = traverseOwnedNodes(
        *sharedPtrBackend_,
        directory,
        cursor,
        startHint,
        lgrInfo.seq,
        reserve,
        {},
        yield,
        [&offers](ripple::SLE&& offer) {
            if (offer.getType() == ripple::ltNFTOKEN_OFFER)
            {
                offers.push_back(std::move(offer));
                return true;
            }

            return false;
        });

    if (auto status = std::get_if<Status>(&result))
        return Error{*status};

    if (offers.size() == reserve)
    {
        output.limit = input.limit;
        output.marker = to_string(offers.back().key());
        offers.pop_back();
    }

    std::move(
        std::begin(offers),
        std::end(offers),
        std::back_inserter(output.offers));

    return std::move(output);
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    NFTOffersHandlerBase::Output const& output)
{
    auto object = boost::json::object{
        {JS(nft_id), output.nftID},
        {JS(validated), output.validated},
        {JS(offers), output.offers},
    };

    if (output.marker)
        object[JS(marker)] = *(output.marker);
    if (output.limit)
        object[JS(limit)] = *(output.limit);

    jv = std::move(object);
}

NFTOffersHandlerBase::Input
tag_invoke(
    boost::json::value_to_tag<NFTOffersHandlerBase::Input>,
    boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    NFTOffersHandlerBase::Input input;

    input.nftID = jsonObject.at(JS(nft_id)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_hash)))
    {
        input.ledgerHash = jsonObject.at(JS(ledger_hash)).as_string().c_str();
    }

    if (jsonObject.contains(JS(ledger_index)))
    {
        if (!jsonObject.at(JS(ledger_index)).is_string())
        {
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        }
        else if (jsonObject.at(JS(ledger_index)).as_string() != "validated")
        {
            input.ledgerIndex =
                std::stoi(jsonObject.at(JS(ledger_index)).as_string().c_str());
        }
    }

    if (jsonObject.contains(JS(marker)))
    {
        input.marker = jsonObject.at(JS(marker)).as_string().c_str();
    }

    if (jsonObject.contains(JS(limit)))
    {
        input.limit = jsonObject.at(JS(limit)).as_int64();
    }

    return input;
}

}  // namespace RPCng
