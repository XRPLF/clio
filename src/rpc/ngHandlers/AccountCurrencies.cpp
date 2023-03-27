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

#include <rpc/ngHandlers/AccountCurrencies.h>

namespace RPCng {
AccountCurrenciesHandler::Result
AccountCurrenciesHandler::process(
    AccountCurrenciesHandler::Input input,
    boost::asio::yield_context& yield) const
{
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
    auto const addToResponse = [&](ripple::SLE&& sle) {
        if (sle.getType() == ripple::ltRIPPLE_STATE)
        {
            ripple::STAmount balance = sle.getFieldAmount(ripple::sfBalance);
            auto const lowLimit = sle.getFieldAmount(ripple::sfLowLimit);
            auto const highLimit = sle.getFieldAmount(ripple::sfHighLimit);
            bool const viewLowest = (lowLimit.getIssuer() == accountID);
            auto const lineLimit = viewLowest ? lowLimit : highLimit;
            auto const lineLimitPeer = !viewLowest ? lowLimit : highLimit;
            if (!viewLowest)
                balance.negate();
            if (balance < lineLimit)
                response.receiveCurrencies.insert(
                    ripple::to_string(balance.getCurrency()));
            if ((-balance) < lineLimitPeer)
                response.sendCurrencies.insert(
                    ripple::to_string(balance.getCurrency()));
        }
        return true;
    };

    // traverse all owned nodes, limit->max, marker->empty
    RPC::ngTraverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,
        lgrInfo.seq,
        std::numeric_limits<std::uint32_t>::max(),
        {},
        yield,
        addToResponse);

    response.ledgerHash = ripple::strHex(lgrInfo.hash);
    response.ledgerIndex = lgrInfo.seq;
    return response;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountCurrenciesHandler::Output const& output)
{
    jv = {
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(receive_currencies), output.receiveCurrencies},
        {JS(send_currencies), output.sendCurrencies}};
}

AccountCurrenciesHandler::Input
tag_invoke(
    boost::json::value_to_tag<AccountCurrenciesHandler::Input>,
    boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    AccountCurrenciesHandler::Input input;
    input.account = jv.at(JS(account)).as_string().c_str();
    if (jsonObject.contains(JS(ledger_hash)))
    {
        input.ledgerHash = jv.at(JS(ledger_hash)).as_string().c_str();
    }
    if (jsonObject.contains(JS(ledger_index)))
    {
        if (!jsonObject.at(JS(ledger_index)).is_string())
        {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        }
        else if (jsonObject.at(JS(ledger_index)).as_string() != "validated")
        {
            input.ledgerIndex =
                std::stoi(jv.at(JS(ledger_index)).as_string().c_str());
        }
    }
    return input;
}

}  // namespace RPCng
