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

#include <ripple/protocol/TxFlags.h>
#include <rpc/handlers/NoRippleCheck.h>

#include <fmt/core.h>

namespace RPC {

NoRippleCheckHandler::Result
NoRippleCheckHandler::process(NoRippleCheckHandler::Input input, Context const& ctx) const
{
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence);

    if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerInfo>(lgrInfoOrStatus);
    auto const accountID = accountFromStringStrict(input.account);
    auto const keylet = ripple::keylet::account(*accountID).key;
    auto const accountObj = sharedPtrBackend_->fetchLedgerObject(keylet, lgrInfo.seq, ctx.yield);

    if (!accountObj)
        return Error{Status{RippledError::rpcACT_NOT_FOUND, "accountNotFound"}};

    auto it = ripple::SerialIter{accountObj->data(), accountObj->size()};
    auto sle = ripple::SLE{it, keylet};
    auto accountSeq = sle.getFieldU32(ripple::sfSequence);
    bool const bDefaultRipple = sle.getFieldU32(ripple::sfFlags) & ripple::lsfDefaultRipple;
    auto const fees = input.transactions ? sharedPtrBackend_->fetchFees(lgrInfo.seq, ctx.yield) : std::nullopt;

    auto output = NoRippleCheckHandler::Output();

    if (input.transactions)
        output.transactions.emplace(boost::json::array());

    auto const getBaseTx = [&](ripple::AccountID const& accountID, std::uint32_t accountSeq) {
        boost::json::object tx;
        tx[JS(Sequence)] = accountSeq;
        tx[JS(Account)] = ripple::toBase58(accountID);
        tx[JS(Fee)] = toBoostJson(fees->units.jsonClipped());

        return tx;
    };

    if (bDefaultRipple && !input.roleGateway)
    {
        output.problems.push_back(
            "You appear to have set your default ripple flag even though "
            "you "
            "are not a gateway. This is not recommended unless you are "
            "experimenting");
    }
    else if (input.roleGateway && !bDefaultRipple)
    {
        output.problems.push_back("You should immediately set your default ripple flag");

        if (input.transactions)
        {
            auto tx = getBaseTx(*accountID, accountSeq++);
            tx[JS(TransactionType)] = "AccountSet";
            tx[JS(SetFlag)] = ripple::asfDefaultRipple;
            output.transactions->push_back(tx);
        }
    }

    auto limit = input.limit;

    ngTraverseOwnedNodes(
        *sharedPtrBackend_,
        *accountID,
        lgrInfo.seq,
        std::numeric_limits<std::uint32_t>::max(),
        {},
        ctx.yield,
        [&](ripple::SLE&& ownedItem) {
            // don't push to result if limit is reached
            if (limit != 0 && ownedItem.getType() == ripple::ltRIPPLE_STATE)
            {
                bool const bLow = accountID == ownedItem.getFieldAmount(ripple::sfLowLimit).getIssuer();

                bool const bNoRipple =
                    ownedItem.getFieldU32(ripple::sfFlags) & (bLow ? ripple::lsfLowNoRipple : ripple::lsfHighNoRipple);

                std::string problem;
                bool needFix = false;
                if (bNoRipple && input.roleGateway)
                {
                    problem = "You should clear the no ripple flag on your ";
                    needFix = true;
                }
                else if (!bNoRipple && !input.roleGateway)
                {
                    problem =
                        "You should probably set the no ripple flag on "
                        "your ";
                    needFix = true;
                }
                if (needFix)
                {
                    --limit;

                    ripple::AccountID peer =
                        ownedItem.getFieldAmount(bLow ? ripple::sfHighLimit : ripple::sfLowLimit).getIssuer();
                    ripple::STAmount peerLimit =
                        ownedItem.getFieldAmount(bLow ? ripple::sfHighLimit : ripple::sfLowLimit);

                    problem += fmt::format(
                        "{} line to {}", to_string(peerLimit.getCurrency()), to_string(peerLimit.getIssuer()));
                    output.problems.emplace_back(problem);

                    if (input.transactions)
                    {
                        ripple::STAmount limitAmount(
                            ownedItem.getFieldAmount(bLow ? ripple::sfLowLimit : ripple::sfHighLimit));
                        limitAmount.setIssuer(peer);

                        auto tx = getBaseTx(*accountID, accountSeq++);

                        tx[JS(TransactionType)] = "TrustSet";
                        tx[JS(LimitAmount)] = toBoostJson(limitAmount.getJson(ripple::JsonOptions::none));
                        tx[JS(Flags)] = bNoRipple ? ripple::tfClearNoRipple : ripple::tfSetNoRipple;

                        output.transactions->push_back(tx);
                    }
                }
            }

            return true;
        });

    output.ledgerIndex = lgrInfo.seq;
    output.ledgerHash = ripple::strHex(lgrInfo.hash);

    return output;
}

NoRippleCheckHandler::Input
tag_invoke(boost::json::value_to_tag<NoRippleCheckHandler::Input>, boost::json::value const& jv)
{
    auto input = NoRippleCheckHandler::Input{};
    auto const& jsonObject = jv.as_object();

    input.account = jsonObject.at(JS(account)).as_string().c_str();
    input.roleGateway = jsonObject.at(JS(role)).as_string() == "gateway";

    if (jsonObject.contains(JS(limit)))
        input.limit = jsonObject.at(JS(limit)).as_int64();

    if (jsonObject.contains(JS(transactions)))
        input.transactions = jsonObject.at(JS(transactions)).as_bool();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = jsonObject.at(JS(ledger_hash)).as_string().c_str();

    if (jsonObject.contains(JS(ledger_index)))
    {
        if (!jsonObject.at(JS(ledger_index)).is_string())
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        else if (jsonObject.at(JS(ledger_index)).as_string() != "validated")
            input.ledgerIndex = std::stoi(jsonObject.at(JS(ledger_index)).as_string().c_str());
    }

    return input;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, NoRippleCheckHandler::Output const& output)
{
    auto obj = boost::json::object{
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {"problems", output.problems},
    };

    if (output.transactions)
        obj.emplace(JS(transactions), *(output.transactions));

    jv = std::move(obj);
}

}  // namespace RPC
