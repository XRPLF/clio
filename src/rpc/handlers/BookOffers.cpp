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

#include "rpc/handlers/BookOffers.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/beast/utility/Zero.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <string>
#include <variant>

namespace rpc {

BookOffersHandler::Result
BookOffersHandler::process(Input input, Context const& ctx) const
{
    auto bookMaybe = parseBook(input.paysCurrency, input.paysID, input.getsCurrency, input.getsID);
    if (auto const status = std::get_if<Status>(&bookMaybe))
        return Error{*status};

    // check ledger
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    auto const book = std::get<ripple::Book>(bookMaybe);
    auto const bookKey = getBookBase(book);

    // TODO: Add perfomance metrics if needed in future
    auto [offers, _] = sharedPtrBackend_->fetchBookOffers(bookKey, lgrInfo.seq, input.limit, ctx.yield);

    auto output = BookOffersHandler::Output{};
    output.ledgerHash = ripple::strHex(lgrInfo.hash);
    output.ledgerIndex = lgrInfo.seq;
    output.offers = postProcessOrderBook(
        offers, book, input.taker ? *(input.taker) : beast::zero, *sharedPtrBackend_, lgrInfo.seq, ctx.yield
    );

    return output;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, BookOffersHandler::Output const& output)
{
    jv = boost::json::object{
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(offers), output.offers},
    };
}

BookOffersHandler::Input
tag_invoke(boost::json::value_to_tag<BookOffersHandler::Input>, boost::json::value const& jv)
{
    auto input = BookOffersHandler::Input{};
    auto const& jsonObject = jv.as_object();

    ripple::to_currency(
        input.getsCurrency, boost::json::value_to<std::string>(jv.at(JS(taker_gets)).as_object().at(JS(currency)))
    );
    ripple::to_currency(
        input.paysCurrency, boost::json::value_to<std::string>(jv.at(JS(taker_pays)).as_object().at(JS(currency)))
    );

    if (jv.at(JS(taker_gets)).as_object().contains(JS(issuer))) {
        ripple::to_issuer(
            input.getsID, boost::json::value_to<std::string>(jv.at(JS(taker_gets)).as_object().at(JS(issuer)))
        );
    }

    if (jv.at(JS(taker_pays)).as_object().contains(JS(issuer))) {
        ripple::to_issuer(
            input.paysID, boost::json::value_to<std::string>(jv.at(JS(taker_pays)).as_object().at(JS(issuer)))
        );
    }

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jv.at(JS(ledger_index))));
        }
    }

    if (jsonObject.contains(JS(taker)))
        input.taker = accountFromStringStrict(boost::json::value_to<std::string>(jv.at(JS(taker))));

    if (jsonObject.contains(JS(limit)))
        input.limit = jv.at(JS(limit)).as_int64();

    return input;
}

}  // namespace rpc
