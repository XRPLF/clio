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

#include "rpc/handlers/BookChanges.hpp"

#include "data/Types.hpp"
#include "rpc/BookChangesHelper.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/jss.h>

#include <string>
#include <variant>
#include <vector>

namespace rpc {

BookChangesHandler::Result
BookChangesHandler::process(BookChangesHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    auto const transactions = sharedPtrBackend_->fetchAllTransactionsInLedger(lgrInfo.seq, ctx.yield);

    Output response;
    response.bookChanges = BookChanges::compute(transactions);
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;
    response.ledgerTime = lgrInfo.closeTime.time_since_epoch().count();

    return response;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, BookChangesHandler::Output const& output)
{
    using boost::json::value_from;

    jv = {
        {JS(type), "bookChanges"},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(ledger_time), output.ledgerTime},
        {JS(validated), output.validated},
        {JS(changes), value_from(output.bookChanges)},
    };
}

BookChangesHandler::Input
tag_invoke(boost::json::value_to_tag<BookChangesHandler::Input>, boost::json::value const& jv)
{
    auto input = BookChangesHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jv.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(jv.at(JS(ledger_index)).as_string().c_str());
        }
    }

    return input;
}

[[nodiscard]] boost::json::object
computeBookChanges(ripple::LedgerHeader const& lgrInfo, std::vector<data::TransactionAndMetadata> const& transactions)
{
    using boost::json::value_from;

    return {
        {JS(type), "bookChanges"},
        {JS(ledger_index), lgrInfo.seq},
        {JS(ledger_hash), to_string(lgrInfo.hash)},
        {JS(ledger_time), lgrInfo.closeTime.time_since_epoch().count()},
        {JS(changes), value_from(BookChanges::compute(transactions))},
    };
}

}  // namespace rpc
