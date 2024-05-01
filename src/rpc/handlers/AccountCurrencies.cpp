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

#include "rpc/handlers/AccountCurrencies.hpp"

#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/jss.h>

#include <cstdint>
#include <limits>
#include <string>
#include <variant>

namespace rpc {
AccountCurrenciesHandler::Result
AccountCurrenciesHandler::process(AccountCurrenciesHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
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

    Output response;
    auto const addToResponse = [&](ripple::SLE const sle) {
        if (sle.getType() == ripple::ltRIPPLE_STATE) {
            auto balance = sle.getFieldAmount(ripple::sfBalance);
            auto const lowLimit = sle.getFieldAmount(ripple::sfLowLimit);
            auto const highLimit = sle.getFieldAmount(ripple::sfHighLimit);
            bool const viewLowest = (lowLimit.getIssuer() == accountID);
            auto const lineLimit = viewLowest ? lowLimit : highLimit;
            auto const lineLimitPeer = !viewLowest ? lowLimit : highLimit;

            if (!viewLowest)
                balance.negate();

            if (balance < lineLimit)
                response.receiveCurrencies.insert(ripple::to_string(balance.getCurrency()));

            if ((-balance) < lineLimitPeer)
                response.sendCurrencies.insert(ripple::to_string(balance.getCurrency()));
        }

        return true;
    };

    // traverse all owned nodes, limit->max, marker->empty
    traverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,
        lgrInfo.seq,
        std::numeric_limits<std::uint32_t>::max(),
        {},
        ctx.yield,
        addToResponse
    );

    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;

    return response;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountCurrenciesHandler::Output const& output)
{
    using boost::json::value_from;

    jv = {
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(receive_currencies), value_from(output.receiveCurrencies)},
        {JS(send_currencies), value_from(output.sendCurrencies)},
    };
}

AccountCurrenciesHandler::Input
tag_invoke(boost::json::value_to_tag<AccountCurrenciesHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountCurrenciesHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = boost::json::value_to<std::string>(jv.at(JS(account)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jv.at(JS(ledger_index))));
        }
    }

    return input;
}

}  // namespace rpc
