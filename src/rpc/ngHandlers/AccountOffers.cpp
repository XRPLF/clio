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

#include <rpc/ngHandlers/AccountOffers.h>

namespace RPCng {

void
AccountOffersHandler::addOffer(
    std::vector<Offer>& offers,
    ripple::SLE const& offerSle) const
{
    AccountOffersHandler::Offer offer;
    offer.takerPays = offerSle.getFieldAmount(ripple::sfTakerPays);
    offer.takerGets = offerSle.getFieldAmount(ripple::sfTakerGets);

    offer.seq = offerSle.getFieldU32(ripple::sfSequence);
    offer.flags = offerSle.getFieldU32(ripple::sfFlags);
    auto const quality =
        getQuality(offerSle.getFieldH256(ripple::sfBookDirectory));
    ripple::STAmount const rate = ripple::amountFromQuality(quality);
    offer.quality = rate.getText();
    if (offerSle.isFieldPresent(ripple::sfExpiration))
        offer.expiration = offerSle.getFieldU32(ripple::sfExpiration);
    offers.push_back(offer);
};

AccountOffersHandler::Result
AccountOffersHandler::process(
    AccountOffersHandler::Input input,
    Context const& ctx) const
{
    auto& yield = *(ctx.pYield);
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = RPC::getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_,
        yield,
        input.ledgerHash,
        input.ledgerIndex,
        range->maxSequence);

    if (auto const status = std::get_if<RPC::Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerInfo>(lgrInfoOrStatus);

    auto const accountID = RPC::accountFromStringStrict(input.account);

    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        ripple::keylet::account(*accountID).key, lgrInfo.seq, yield);
    if (!accountLedgerObject)
        return Error{RPC::Status{
            RPC::RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    Output response;
    response.account = ripple::to_string(*accountID);
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    auto const addToResponse = [&](ripple::SLE&& sle) {
        if (sle.getType() == ripple::ltOFFER)
        {
            addOffer(response.offers, sle);
        }

        return true;
    };

    auto const next = RPC::ngTraverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,
        lgrInfo.seq,
        input.limit,
        input.marker,
        yield,
        addToResponse);

    if (auto const status = std::get_if<RPC::Status>(&next))
        return Error{*status};

    auto const nextMarker = std::get<RPC::AccountCursor>(next);

    if (nextMarker.isNonZero())
        response.marker = nextMarker.toString();

    return response;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountOffersHandler::Output const& output)
{
    jv = {
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(account), output.account},
        {JS(offers), boost::json::value_from(output.offers)}};
    if (output.marker)
        jv.as_object()[JS(marker)] = *output.marker;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountOffersHandler::Offer const& offer)
{
    jv = {
        {JS(seq), offer.seq},
        {JS(flags), offer.flags},
        {JS(quality), offer.quality}};
    auto& jsonObject = jv.as_object();
    if (offer.expiration)
        jsonObject[JS(expiration)] = *offer.expiration;

    auto const convertAmount = [&](const char* field,
                                   ripple::STAmount const& amount) {
        if (amount.native())
        {
            jsonObject[field] = amount.getText();
        }
        else
        {
            jsonObject[field] = {
                {JS(currency), ripple::to_string(amount.getCurrency())},
                {JS(issuer), ripple::to_string(amount.getIssuer())},
                {JS(value), amount.getText()}};
        }
    };
    convertAmount(JS(taker_pays), offer.takerPays);
    convertAmount(JS(taker_gets), offer.takerGets);
}

AccountOffersHandler::Input
tag_invoke(
    boost::json::value_to_tag<AccountOffersHandler::Input>,
    boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    AccountOffersHandler::Input input;
    input.account = jsonObject.at(JS(account)).as_string().c_str();
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
    if (jsonObject.contains(JS(limit)))
        input.limit = jsonObject.at(JS(limit)).as_int64();
    if (jsonObject.contains(JS(marker)))
        input.marker = jsonObject.at(JS(marker)).as_string().c_str();
    return input;
}

}  // namespace RPCng
