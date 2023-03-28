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

#include <rpc/ngHandlers/GatewayBalances.h>

namespace RPCng {

GatewayBalancesHandler::Result
GatewayBalancesHandler::process(
    GatewayBalancesHandler::Input input,
    boost::asio::yield_context& yield) const
{
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

    // check account
    auto const lgrInfo = std::get<ripple::LedgerInfo>(lgrInfoOrStatus);
    auto const accountID = RPC::accountFromStringStrict(input.account);
    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(
        ripple::keylet::account(*accountID).key, lgrInfo.seq, yield);
    if (!accountLedgerObject)
        return Error{RPC::Status{
            RPC::RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    GatewayBalancesHandler::Output output;

    auto const addToResponse = [&](ripple::SLE&& sle) {
        if (sle.getType() == ripple::ltRIPPLE_STATE)
        {
            ripple::STAmount balance = sle.getFieldAmount(ripple::sfBalance);
            auto const lowLimit = sle.getFieldAmount(ripple::sfLowLimit);
            auto const highLimit = sle.getFieldAmount(ripple::sfHighLimit);
            auto const lowID = lowLimit.getIssuer();
            auto const highID = highLimit.getIssuer();
            auto const viewLowest = (lowLimit.getIssuer() == accountID);
            auto const flags = sle.getFieldU32(ripple::sfFlags);
            auto const freeze = flags &
                (viewLowest ? ripple::lsfLowFreeze : ripple::lsfHighFreeze);
            if (!viewLowest)
                balance.negate();

            auto const balSign = balance.signum();
            if (balSign == 0)
                return true;

            auto const& peer = !viewLowest ? lowID : highID;

            // Here, a negative balance means the cold wallet owes (normal)
            // A positive balance means the cold wallet has an asset
            // (unusual)

            if (input.hotWallets.count(peer) > 0)
            {
                // This is a specified hot wallet
                output.hotBalances[peer].push_back(-balance);
            }
            else if (balSign > 0)
            {
                // This is a gateway asset
                output.assets[peer].push_back(balance);
            }
            else if (freeze)
            {
                // An obligation the gateway has frozen
                output.frozenBalances[peer].push_back(-balance);
            }
            else
            {
                // normal negative balance, obligation to customer
                auto& bal = output.sums[balance.getCurrency()];
                if (bal == beast::zero)
                {
                    // This is needed to set the currency code correctly
                    bal = -balance;
                }
                else
                {
                    try
                    {
                        bal -= balance;
                    }
                    catch (std::runtime_error const& e)
                    {
                        output.overflow = true;
                    }
                }
            }
        }
        return true;
    };

    // traverse all owned nodes, limit->max, marker->empty
    auto const ret = RPC::ngTraverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,
        lgrInfo.seq,
        std::numeric_limits<std::uint32_t>::max(),
        {},
        yield,
        addToResponse);

    if (auto status = std::get_if<RPC::Status>(&ret))
        return Error{*status};

    if (not std::all_of(
            input.hotWallets.begin(),
            input.hotWallets.end(),
            [&](auto const& hw) { return output.hotBalances.contains(hw); }))
        return Error{RPC::Status{
            RPC::RippledError::rpcINVALID_PARAMS, "invalidHotWallet"}};

    output.accountID = input.account;
    output.ledgerHash = ripple::strHex(lgrInfo.hash);
    output.ledgerIndex = lgrInfo.seq;
    return output;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    GatewayBalancesHandler::Output const& output)
{
    boost::json::object obj;
    if (!output.sums.empty())
    {
        boost::json::object obligations;
        for (auto const& [k, v] : output.sums)
        {
            obligations[ripple::to_string(k)] = v.getText();
        }
        obj[JS(obligations)] = std::move(obligations);
    }

    auto const toJson =
        [](std::map<ripple::AccountID, std::vector<ripple::STAmount>> const&
               balances) {
            boost::json::object balancesObj;
            if (!balances.empty())
            {
                for (auto const& [accId, accBalances] : balances)
                {
                    boost::json::array arr;
                    for (auto const& balance : accBalances)
                    {
                        boost::json::object entry;
                        entry[JS(currency)] =
                            ripple::to_string(balance.issue().currency);
                        entry[JS(value)] = balance.getText();
                        arr.push_back(std::move(entry));
                    }
                    balancesObj[ripple::to_string(accId)] = std::move(arr);
                }
            }
            return balancesObj;
        };

    if (auto balances = toJson(output.hotBalances); balances.size())
        obj[JS(balances)] = balances;

    // we don't have frozen_balances field in the
    // document:https://xrpl.org/gateway_balances.html#gateway_balances
    if (auto balances = toJson(output.frozenBalances); balances.size())
        obj[JS(frozen_balances)] = balances;
    if (auto balances = toJson(output.assets); balances.size())
        obj[JS(assets)] = balances;
    obj[JS(account)] = output.accountID;
    obj[JS(ledger_index)] = output.ledgerIndex;
    obj[JS(ledger_hash)] = output.ledgerHash;
    if (output.overflow)
        obj["overflow"] = true;
    jv = std::move(obj);
}

GatewayBalancesHandler::Input
tag_invoke(
    boost::json::value_to_tag<GatewayBalancesHandler::Input>,
    boost::json::value const& jv)
{
    auto const& jsonObject = jv.as_object();
    GatewayBalancesHandler::Input input;
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
    if (jsonObject.contains(JS(hotwallet)))
    {
        if (jsonObject.at(JS(hotwallet)).is_string())
        {
            input.hotWallets.insert(*RPC::accountFromStringStrict(
                jv.at(JS(hotwallet)).as_string().c_str()));
        }
        else
        {
            auto const& hotWallets = jv.at(JS(hotwallet)).as_array();
            std::transform(
                hotWallets.begin(),
                hotWallets.end(),
                std::inserter(input.hotWallets, input.hotWallets.begin()),
                [](auto const& hotWallet) {
                    return *RPC::accountFromStringStrict(
                        hotWallet.as_string().c_str());
                });
        }
    }
    return input;
}
}  // namespace RPCng
