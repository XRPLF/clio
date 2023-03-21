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
#include <rpc/ngHandlers/BookOffers.h>

namespace RPCng {

BookOffersHandler::Result
BookOffersHandler::process(Input input, boost::asio::yield_context& yield) const
{
    auto bookMaybe = RPC::parseBook(
        input.paysCurrency, input.paysID, input.getsCurrency, input.getsID);
    if (auto const status = std::get_if<RPC::Status>(&bookMaybe))
        return Error{*status};

    // check ledger
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
    auto const book = std::get<ripple::Book>(bookMaybe);
    auto const bookKey = getBookBase(book);

    // TODO: Add perfomance metrics if needed in future
    auto [offers, _] = sharedPtrBackend_->fetchBookOffers(
        bookKey, lgrInfo.seq, input.limit, yield);

    BookOffersHandler::Output output;
    output.ledgerHash = ripple::strHex(lgrInfo.hash);
    output.ledgerIndex = lgrInfo.seq;
    output.offers = RPC::postProcessOrderBook(
        offers,
        book,
        input.taker ? *(input.taker) : beast::zero,
        *sharedPtrBackend_,
        lgrInfo.seq,
        yield);

    return output;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    BookOffersHandler::Output const& output)
{
    jv = boost::json::object{
        {"ledger_hash", output.ledgerHash},
        {"ledger_index", output.ledgerIndex},
        {"offers", output.offers},
    };
}

BookOffersHandler::Input
tag_invoke(
    boost::json::value_to_tag<BookOffersHandler::Input>,
    boost::json::value const& jv)
{
    BookOffersHandler::Input input;
    auto const& jsonObject = jv.as_object();
    ripple::to_currency(
        input.getsCurrency,
        jv.at("taker_gets").as_object().at("currency").as_string().c_str());
    ripple::to_currency(
        input.paysCurrency,
        jv.at("taker_pays").as_object().at("currency").as_string().c_str());
    if (jv.at("taker_gets").as_object().contains("issuer"))
    {
        ripple::to_issuer(
            input.getsID,
            jv.at("taker_gets").as_object().at("issuer").as_string().c_str());
    }
    if (jv.at("taker_pays").as_object().contains("issuer"))
    {
        ripple::to_issuer(
            input.paysID,
            jv.at("taker_pays").as_object().at("issuer").as_string().c_str());
    }
    if (jsonObject.contains("ledger_hash"))
    {
        input.ledgerHash = jv.at("ledger_hash").as_string().c_str();
    }
    if (jsonObject.contains("ledger_index"))
    {
        if (!jsonObject.at("ledger_index").is_string())
        {
            input.ledgerIndex = jv.at("ledger_index").as_int64();
        }
        else if (jsonObject.at("ledger_index").as_string() != "validated")
        {
            input.ledgerIndex =
                std::stoi(jv.at("ledger_index").as_string().c_str());
        }
    }
    if (jsonObject.contains("taker"))
    {
        input.taker =
            RPC::accountFromStringStrict(jv.at("taker").as_string().c_str());
    }
    if (jsonObject.contains("limit"))
    {
        input.limit = jv.at("limit").as_int64();
    }

    return input;
}

}  // namespace RPCng
