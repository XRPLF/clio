#include "rpc/handlers/AMMInfo.hpp"

#include "data/DBHelpers.hpp"
#include "rpc/AMMHelpers.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <date/date.h>
#include <rpcspec/Errors.hpp>
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
#include <string>
#include <utility>

namespace {

std::string
toIso8601(xrpl::NetClock::time_point tp)
{
    using namespace std::chrono;
    static constexpr auto kRippleEpochOffset = seconds{kRippleEpochStart};

    return date::format(
        "%Y-%Om-%dT%H:%M:%OS%z",
        date::sys_time<system_clock::duration>(
            system_clock::time_point{tp.time_since_epoch() + kRippleEpochOffset}
        )
    );
};

}  // namespace

namespace rpc {

AMMInfoHandler::Result
AMMInfoHandler::process(AMMInfoHandler::Input const& input, Context const& ctx) const
{
    using namespace xrpl;

    auto const hasInvalidParams = [&input] {
        // no asset/asset2 can be specified if amm account is specified
        if (input.ammAccount)
            return input.issue1 != xrpl::noIssue() || input.issue2 != xrpl::noIssue();

        // both assets must be specified when amm account is not specified
        return input.issue1 == xrpl::noIssue() || input.issue2 == xrpl::noIssue();
    }();

    if (hasInvalidParams)
        return Error{Status{RippledError::RpcInvalidParams}};

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AMMInfo's ledger range must be available");

    auto const expectedLgrInfo = getLedgerHeaderFromLedgerSpecifier(
        *sharedPtrBackend_,
        ctx.yield,
        input.ledger,
        range->maxSequence  // NOLINT(bugprone-unchecked-optional-access)
    );

    if (not expectedLgrInfo.has_value())
        return Error{expectedLgrInfo.error()};

    auto const& lgrInfo = *expectedLgrInfo;

    if (input.accountID) {
        auto keylet = keylet::account(*input.accountID);
        if (not sharedPtrBackend_->fetchLedgerObject(keylet.key, lgrInfo.seq, ctx.yield))
            return Error{Status{RippledError::RpcActNotFound}};
    }

    xrpl::uint256 ammID;
    if (input.ammAccount) {
        auto const accountKeylet = keylet::account(*input.ammAccount);
        auto const accountLedgerObject =
            sharedPtrBackend_->fetchLedgerObject(accountKeylet.key, lgrInfo.seq, ctx.yield);
        if (not accountLedgerObject)
            return Error{Status{RippledError::RpcActMalformed}};
        xrpl::STLedgerEntry const sle{
            xrpl::SerialIter{accountLedgerObject->data(), accountLedgerObject->size()},
            accountKeylet.key
        };
        if (not sle.isFieldPresent(xrpl::sfAMMID))
            return Error{Status{RippledError::RpcActNotFound}};
        ammID = sle.getFieldH256(xrpl::sfAMMID);
    }

    auto issue1 = input.issue1;
    auto issue2 = input.issue2;
    auto ammKeylet = ammID != 0 ? keylet::amm(ammID) : keylet::amm(issue1, issue2);
    auto const ammBlob =
        sharedPtrBackend_->fetchLedgerObject(ammKeylet.key, lgrInfo.seq, ctx.yield);

    if (not ammBlob)
        return Error{Status{RippledError::RpcActNotFound}};

    auto const amm = SLE{SerialIter{ammBlob->data(), ammBlob->size()}, ammKeylet.key};
    auto const ammAccountID = amm.getAccountID(sfAccount);
    auto const accBlob = sharedPtrBackend_->fetchLedgerObject(
        keylet::account(ammAccountID).key, lgrInfo.seq, ctx.yield
    );
    if (not accBlob)
        return Error{Status{RippledError::RpcActNotFound}};

    // If the issue1 and issue2 are not specified, we need to get them from the AMM.
    // Otherwise we preserve the mapping of asset1 -> issue1 and asset2 -> issue2 as requested by
    // the user.
    if (issue1 == xrpl::noIssue() and issue2 == xrpl::noIssue()) {
        issue1 = amm[sfAsset].get<Issue>();
        issue2 = amm[sfAsset2].get<Issue>();
    }

    auto const [asset1Balance, asset2Balance] = getAmmPoolHolds(
        *sharedPtrBackend_,
        *amendmentCenter_,
        lgrInfo.seq,
        ammAccountID,
        issue1,
        issue2,
        false,
        ctx.yield
    );
    auto const lptAMMBalance = input.accountID
        ? getAmmLpHolds(*sharedPtrBackend_, lgrInfo.seq, amm, *input.accountID, ctx.yield)
        : amm[sfLPTokenBalance];

    Output response;
    response.ledgerIndex = lgrInfo.seq;
    response.ledgerHash = xrpl::strHex(lgrInfo.hash);
    response.amount1 = toBoostJson(asset1Balance.getJson(JsonOptions::Values::None));
    response.amount2 = toBoostJson(asset2Balance.getJson(JsonOptions::Values::None));
    response.lpToken = toBoostJson(lptAMMBalance.getJson(JsonOptions::Values::None));
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
            auto const timeSlot =
                ammAuctionTimeSlot(lgrInfo.parentCloseTime.time_since_epoch().count(), auctionSlot);

            auction[JS(time_interval)] = timeSlot ? *timeSlot : xrpl::kAuctionSlotTimeIntervals;
            auction[JS(price)] =
                toBoostJson(auctionSlot[sfPrice].getJson(JsonOptions::Values::None));
            auction[JS(discounted_fee)] = auctionSlot[sfDiscountedFee];
            auction[JS(account)] = to_string(auctionSlot.getAccountID(sfAccount));
            auction[JS(expiration)] =
                toIso8601(NetClock::time_point{NetClock::duration{auctionSlot[sfExpiration]}});

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
            *sharedPtrBackend_,
            lgrInfo.seq,
            ammAccountID,
            amm[sfAsset].get<Issue>().currency,
            amm[sfAsset].get<Issue>().account,
            ctx.yield
        );
    }
    if (!isXRP(asset2Balance)) {
        response.asset2Frozen = isFrozen(
            *sharedPtrBackend_,
            lgrInfo.seq,
            ammAccountID,
            amm[sfAsset2].get<Issue>().currency,
            amm[sfAsset2].get<Issue>().account,
            ctx.yield
        );
    }

    return response;
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AMMInfoHandler::Output const& output
)
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

}  // namespace rpc
