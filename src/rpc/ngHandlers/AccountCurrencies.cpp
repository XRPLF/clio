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
#include <rpc/ngHandlers/AccountCurrencies.h>

namespace RPCng {
AccountCurrenciesHandler::Result
AccountCurrenciesHandler::process(AccountCurrenciesHandler::Input input) const
{
    auto range = sharedPtrBackend_->fetchLedgerRange();
    auto lgrInfoOrStatus = RPC::getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_,
        yieldCtx_,
        input.ledgerHash,
        input.ledgerIndex,
        range->maxSequence);

    if (auto status = std::get_if<RPC::Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto lgrInfo = std::get<ripple::LedgerInfo>(lgrInfoOrStatus);

    auto accountID = RPC::accountFromStringStrict(input.account);

    auto accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        ripple::keylet::account(*accountID).key, lgrInfo.seq, yieldCtx_);
    if (!accountLedgerObject)
        return Error{RPC::Status{
            RPC::RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    Output out;
    auto const addToResponse = [&](ripple::SLE&& sle) {
        if (sle.getType() == ripple::ltRIPPLE_STATE)
        {
            ripple::STAmount balance = sle.getFieldAmount(ripple::sfBalance);
            auto lowLimit = sle.getFieldAmount(ripple::sfLowLimit);
            auto highLimit = sle.getFieldAmount(ripple::sfHighLimit);
            bool viewLowest = (lowLimit.getIssuer() == accountID);
            auto lineLimit = viewLowest ? lowLimit : highLimit;
            auto lineLimitPeer = !viewLowest ? lowLimit : highLimit;
            if (!viewLowest)
                balance.negate();
            if (balance < lineLimit)
                out.receiveCurrencies.insert(
                    ripple::to_string(balance.getCurrency()));
            if ((-balance) < lineLimitPeer)
                out.sendCurrencies.insert(
                    ripple::to_string(balance.getCurrency()));
        }
        return true;
    };

    RPC::traverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,
        lgrInfo.seq,
        std::numeric_limits<std::uint32_t>::max(),
        {},
        yieldCtx_,
        addToResponse);

    out.ledgerHash = ripple::strHex(lgrInfo.hash);
    out.ledgerIndex = lgrInfo.seq;
    return out;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountCurrenciesHandler::Output output)
{
    boost::json::object obj;
    obj = {
        {"ledger_hash", output.ledgerHash},
        {"ledger_index", output.ledgerIndex},
        {"validated", output.validated},
        {"receive_currencies",
         boost::json::value_from(output.receiveCurrencies)},
        {"send_currencies", boost::json::value_from(output.sendCurrencies)}};
    jv.emplace_object() = obj;
}

AccountCurrenciesHandler::Input
tag_invoke(
    boost::json::value_to_tag<AccountCurrenciesHandler::Input>,
    boost::json::value const& jv)
{
    auto jsonObject = jv.as_object();
    AccountCurrenciesHandler::Input input;
    input.account = jv.at("account").as_string().c_str();
    if (jsonObject.contains("ledger_hash"))
    {
        input.ledgerHash = jv.at("ledger_hash").as_string().c_str();
    }
    if (jsonObject.contains("ledger_index"))
    {
        if (!jsonObject["ledger_index"].is_string())
        {
            input.ledgerIndex = jv.at("ledger_index").as_uint64();
        }
        else if (jsonObject["ledger_index"].as_string() != "validated")
        {
            input.ledgerIndex =
                std::stoi(jv.at("ledger_index").as_string().c_str());
        }
    }

    return input;
}

}  // namespace RPCng
