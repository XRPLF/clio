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

#include "rpc/handlers/AMMInfo.h"

#include "data/DBHelpers.h"
#include "rpc/AMMHelpers.h"

#include <ripple/protocol/AMMCore.h>

namespace {

std::string
toIso8601(ripple::NetClock::time_point tp)
{
    using namespace std::chrono;
    static auto constexpr rippleEpochOffset = seconds{rippleEpochStart};

    return date::format(
        "%Y-%Om-%dT%H:%M:%OS%z",
        date::sys_time<system_clock::duration>(system_clock::time_point{tp.time_since_epoch() + rippleEpochOffset})
    );
};

}  // namespace

namespace rpc {
AMMInfoHandler::Result
AMMInfoHandler::process(AMMInfoHandler::Input input, Context const& ctx) const
{
    using namespace ripple;

    auto const hasInvalidParams = [&input] {
        // no asset/asset2 can be specified if amm account is specified
        if (input.ammAccount)
            return input.issue1 != ripple::noIssue() || input.issue2 != ripple::noIssue();

        // both assets must be specified when amm account is not specified
        return input.issue1 == ripple::noIssue() || input.issue2 == ripple::noIssue();
    }();

    if (hasInvalidParams)
        return Error{Status{RippledError::rpcINVALID_PARAMS}};

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<LedgerInfo>(lgrInfoOrStatus);

    if (input.accountID) {
        auto keylet = keylet::account(*input.accountID);
        if (not sharedPtrBackend_->fetchLedgerObject(keylet.key, lgrInfo.seq, ctx.yield))
            return Error{Status{RippledError::rpcACT_NOT_FOUND, "Account not found."}};
    }

    ripple::uint256 ammID;
    if (input.ammAccount) {
        auto const accountKeylet = keylet::account(*input.ammAccount);
        auto const accountLedgerObject =
            sharedPtrBackend_->fetchLedgerObject(accountKeylet.key, lgrInfo.seq, ctx.yield);
        if (not accountLedgerObject)
            return Error{Status{RippledError::rpcACT_MALFORMED, "Amm account malformed."}};
        ripple::STLedgerEntry const sle{
            ripple::SerialIter{accountLedgerObject->data(), accountLedgerObject->size()}, accountKeylet.key
        };
        if (not sle.isFieldPresent(ripple::sfAMMID))
            return Error{Status{RippledError::rpcACT_NOT_FOUND, "Amm account not found."}};
        ammID = sle.getFieldH256(ripple::sfAMMID);
    }

    auto ammKeylet = ammID != 0 ? keylet::amm(ammID) : keylet::amm(input.issue1, input.issue2);
    auto const ammBlob = sharedPtrBackend_->fetchLedgerObject(ammKeylet.key, lgrInfo.seq, ctx.yield);

    if (not ammBlob)
        return Error{Status{RippledError::rpcACT_NOT_FOUND, "Amm account not found."}};

    auto const amm = SLE{SerialIter{ammBlob->data(), ammBlob->size()}, ammKeylet.key};
    auto const ammAccountID = amm.getAccountID(sfAccount);
    auto const accBlob =
        sharedPtrBackend_->fetchLedgerObject(keylet::account(ammAccountID).key, lgrInfo.seq, ctx.yield);
    if (not accBlob)
        return Error{Status{RippledError::rpcACT_NOT_FOUND, "Amm account not found."}};

    auto const [asset1Balance, asset2Balance] =
        getAmmPoolHolds(*sharedPtrBackend_, lgrInfo.seq, ammAccountID, amm[sfAsset], amm[sfAsset2], false, ctx.yield);
    auto const lptAMMBalance = input.accountID
        ? getAmmLpHolds(*sharedPtrBackend_, lgrInfo.seq, amm, *input.accountID, ctx.yield)
        : amm[sfLPTokenBalance];

    Output response;
    response.ledgerIndex = lgrInfo.seq;
    response.amount1 = toBoostJson(asset1Balance.getJson(JsonOptions::none));
    response.amount2 = toBoostJson(asset2Balance.getJson(JsonOptions::none));
    response.lpToken = toBoostJson(lptAMMBalance.getJson(JsonOptions::none));
    response.tradingFee = amm[sfTradingFee];
    response.ammAccount = to_string(ammAccountID);

    if (amm.isFieldPresent(sfVoteSlots)) {
        for (auto const& voteEntry : amm.getFieldArray(sfVoteSlots)) {
            boost::json::object vote;
            vote[JS(account)] = to_string(voteEntry.getAccountID(sfAccount));
            vote[JS(trading_fee)] = voteEntry[sfTradingFee];
            vote[JS(vote_weight)] = voteEntry[sfVoteWeight];

            response.voteSlots.push_back(std::move(vote));
        }
    }

    if (amm.isFieldPresent(sfAuctionSlot)) {
        auto const& auctionSlot = amm.peekAtField(sfAuctionSlot).downcast<STObject>();
        if (auctionSlot.isFieldPresent(sfAccount)) {
            boost::json::object auction;
            auto const timeSlot = ammAuctionTimeSlot(lgrInfo.parentCloseTime.time_since_epoch().count(), auctionSlot);

            auction[JS(time_interval)] = timeSlot ? *timeSlot : AUCTION_SLOT_TIME_INTERVALS;
            auction[JS(price)] = toBoostJson(auctionSlot[sfPrice].getJson(JsonOptions::none));
            auction[JS(discounted_fee)] = auctionSlot[sfDiscountedFee];
            auction[JS(account)] = to_string(auctionSlot.getAccountID(sfAccount));
            auction[JS(expiration)] = toIso8601(NetClock::time_point{NetClock::duration{auctionSlot[sfExpiration]}});

            if (auctionSlot.isFieldPresent(sfAuthAccounts)) {
                boost::json::array auth;
                for (auto const& acct : auctionSlot.getFieldArray(sfAuthAccounts)) {
                    boost::json::object accountData;
                    accountData[JS(account)] = to_string(acct.getAccountID(sfAccount));
                    auth.push_back(accountData);
                }

                auction[JS(auth_accounts)] = std::move(auth);
            }

            response.auctionSlot = std::move(auction);
        }
    }

    if (!isXRP(asset1Balance)) {
        response.asset1Frozen = isFrozen(
            *sharedPtrBackend_, lgrInfo.seq, ammAccountID, amm[sfAsset].currency, amm[sfAsset].account, ctx.yield
        );
    }
    if (!isXRP(asset2Balance)) {
        response.asset2Frozen = isFrozen(
            *sharedPtrBackend_, lgrInfo.seq, ammAccountID, amm[sfAsset2].currency, amm[sfAsset2].account, ctx.yield
        );
    }

    return response;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AMMInfoHandler::Output const& output)
{
    boost::json::object amm = {
        {JS(lp_token), output.lpToken},
        {JS(amount), output.amount1},
        {JS(amount2), output.amount2},
        {JS(account), output.ammAccount},
        {JS(trading_fee), output.tradingFee},
    };

    if (output.auctionSlot != nullptr)
        amm[JS(auction_slot)] = output.auctionSlot;

    if (not output.voteSlots.empty())
        amm[JS(vote_slots)] = output.voteSlots;

    if (output.asset1Frozen)
        amm[JS(asset_frozen)] = *output.asset1Frozen;

    if (output.asset2Frozen)
        amm[JS(asset2_frozen)] = *output.asset2Frozen;

    jv = {
        {JS(amm), amm},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
    };
}

AMMInfoHandler::Input
tag_invoke(boost::json::value_to_tag<AMMInfoHandler::Input>, boost::json::value const& jv)
{
    auto input = AMMInfoHandler::Input{};
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

    auto getIssue = [](boost::json::value const& request) {
        if (request.is_string())
            return ripple::issueFromJson(request.as_string().c_str());

        // Note: no checks needed as we already validated the input if we made it here
        auto const currency = ripple::to_currency(request.at(JS(currency)).as_string().c_str());
        if (ripple::isXRP(currency)) {
            return ripple::xrpIssue();
        }
        auto const issuer = ripple::parseBase58<ripple::AccountID>(request.at(JS(issuer)).as_string().c_str());
        return ripple::Issue{currency, *issuer};
    };

    if (jsonObject.contains(JS(asset)))
        input.issue1 = getIssue(jsonObject.at(JS(asset)));

    if (jsonObject.contains(JS(asset2)))
        input.issue2 = getIssue(jsonObject.at(JS(asset2)));

    if (jsonObject.contains(JS(account)))
        input.accountID = accountFromStringStrict(jsonObject.at(JS(account)).as_string().c_str());
    if (jsonObject.contains(JS(amm_account)))
        input.ammAccount = accountFromStringStrict(jsonObject.at(JS(amm_account)).as_string().c_str());

    return input;
}

}  // namespace rpc
