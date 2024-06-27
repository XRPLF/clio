//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "rpc/handlers/AMMInfo.hpp"

#include "data/DBHelpers.hpp"
#include "rpc/AMMHelpers.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <date/date.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/chrono.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/AMMCore.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

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
    auto const lgrInfoOrStatus = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<LedgerHeader>(lgrInfoOrStatus);

    if (input.accountID) {
        auto keylet = keylet::account(*input.accountID);
        if (not sharedPtrBackend_->fetchLedgerObject(keylet.key, lgrInfo.seq, ctx.yield))
            return Error{Status{RippledError::rpcACT_NOT_FOUND}};
    }

    ripple::uint256 ammID;
    if (input.ammAccount) {
        auto const accountKeylet = keylet::account(*input.ammAccount);
        auto const accountLedgerObject =
            sharedPtrBackend_->fetchLedgerObject(accountKeylet.key, lgrInfo.seq, ctx.yield);
        if (not accountLedgerObject)
            return Error{Status{RippledError::rpcACT_MALFORMED}};
        ripple::STLedgerEntry const sle{
            ripple::SerialIter{accountLedgerObject->data(), accountLedgerObject->size()}, accountKeylet.key
        };
        if (not sle.isFieldPresent(ripple::sfAMMID))
            return Error{Status{RippledError::rpcACT_NOT_FOUND}};
        ammID = sle.getFieldH256(ripple::sfAMMID);
    }

    auto issue1 = input.issue1;
    auto issue2 = input.issue2;
    auto ammKeylet = ammID != 0 ? keylet::amm(ammID) : keylet::amm(issue1, issue2);
    auto const ammBlob = sharedPtrBackend_->fetchLedgerObject(ammKeylet.key, lgrInfo.seq, ctx.yield);

    if (not ammBlob)
        return Error{Status{RippledError::rpcACT_NOT_FOUND}};

    auto const amm = SLE{SerialIter{ammBlob->data(), ammBlob->size()}, ammKeylet.key};
    auto const ammAccountID = amm.getAccountID(sfAccount);
    auto const accBlob =
        sharedPtrBackend_->fetchLedgerObject(keylet::account(ammAccountID).key, lgrInfo.seq, ctx.yield);
    if (not accBlob)
        return Error{Status{RippledError::rpcACT_NOT_FOUND}};

    // If the issue1 and issue2 are not specified, we need to get them from the AMM.
    // Otherwise we preserve the mapping of asset1 -> issue1 and asset2 -> issue2 as requested by the user.
    if (issue1 == ripple::noIssue() and issue2 == ripple::noIssue()) {
        issue1 = amm[sfAsset];
        issue2 = amm[sfAsset2];
    }

    auto const [asset1Balance, asset2Balance] =
        getAmmPoolHolds(*sharedPtrBackend_, lgrInfo.seq, ammAccountID, issue1, issue2, false, ctx.yield);
    auto const lptAMMBalance = input.accountID
        ? getAmmLpHolds(*sharedPtrBackend_, lgrInfo.seq, amm, *input.accountID, ctx.yield)
        : amm[sfLPTokenBalance];

    Output response;
    response.ledgerIndex = lgrInfo.seq;
    response.ledgerHash = ripple::strHex(lgrInfo.hash);
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
                    auth.push_back(std::move(accountData));
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

RpcSpecConstRef
AMMInfoHandler::spec([[maybe_unused]] uint32_t apiVersion)
{
    static auto const stringIssueValidator =
        validation::CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
            if (not value.is_string())
                return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

            try {
                ripple::issueFromJson(boost::json::value_to<std::string>(value));
            } catch (std::runtime_error const&) {
                return Error{Status{RippledError::rpcISSUE_MALFORMED}};
            }

            return MaybeError{};
        }};

    static auto const rpcSpec = RpcSpec{
        {JS(ledger_hash), validation::CustomValidators::Uint256HexStringValidator},
        {JS(ledger_index), validation::CustomValidators::LedgerIndexValidator},
        {JS(asset),
         meta::WithCustomError{
             validation::Type<std::string, boost::json::object>{}, Status(RippledError::rpcISSUE_MALFORMED)
         },
         meta::IfType<std::string>{stringIssueValidator},
         meta::IfType<boost::json::object>{
             meta::WithCustomError{
                 validation::CustomValidators::CurrencyIssueValidator, Status(RippledError::rpcISSUE_MALFORMED)
             },
         }},
        {JS(asset2),
         meta::WithCustomError{
             validation::Type<std::string, boost::json::object>{}, Status(RippledError::rpcISSUE_MALFORMED)
         },
         meta::IfType<std::string>{stringIssueValidator},
         meta::IfType<boost::json::object>{
             meta::WithCustomError{
                 validation::CustomValidators::CurrencyIssueValidator, Status(RippledError::rpcISSUE_MALFORMED)
             },
         }},
        {JS(amm_account),
         meta::WithCustomError{validation::CustomValidators::AccountValidator, Status(RippledError::rpcACT_MALFORMED)}},
        {JS(account),
         meta::WithCustomError{validation::CustomValidators::AccountValidator, Status(RippledError::rpcACT_MALFORMED)}},
    };

    return rpcSpec;
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
        {JS(ledger_hash), output.ledgerHash},
        {JS(validated), output.validated},
    };
}

AMMInfoHandler::Input
tag_invoke(boost::json::value_to_tag<AMMInfoHandler::Input>, boost::json::value const& jv)
{
    auto input = AMMInfoHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jv.at(JS(ledger_index))));
        }
    }

    if (jsonObject.contains(JS(asset)))
        input.issue1 = parseIssue(jsonObject.at(JS(asset)).as_object());

    if (jsonObject.contains(JS(asset2)))
        input.issue2 = parseIssue(jsonObject.at(JS(asset2)).as_object());

    if (jsonObject.contains(JS(account)))
        input.accountID = accountFromStringStrict(boost::json::value_to<std::string>(jsonObject.at(JS(account))));
    if (jsonObject.contains(JS(amm_account)))
        input.ammAccount = accountFromStringStrict(boost::json::value_to<std::string>(jsonObject.at(JS(amm_account))));

    return input;
}

}  // namespace rpc
