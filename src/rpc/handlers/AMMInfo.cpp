#include <rpc/AMMHelpers.h>

namespace RPC {

ripple::Expected<ripple::Issue, ripple::error_code_i>
getIssue(boost::json::value const& request)
{
    if (!request.is_object())
        return ripple::Unexpected(ripple::rpcAMM_ISSUE_MALFORMED);

    ripple::Issue issue = ripple::xrpIssue();
    if (!request.as_object().contains(JS(currency)))
        return ripple::Unexpected(ripple::rpcAMM_ISSUE_MALFORMED);

    auto const& currency = request.at(JS(currency));
    if (!ripple::to_currency(issue.currency, currency.as_string().c_str()))
        return ripple::Unexpected(ripple::rpcAMM_ISSUE_MALFORMED);

    if (isXRP(issue.currency))
    {
        if (request.as_object().contains(JS(issuer)))
            return ripple::Unexpected(ripple::rpcAMM_ISSUE_MALFORMED);

        return issue;
    }

    if (!request.as_object().contains(JS(issuer)))
        return ripple::Unexpected(ripple::rpcAMM_ISSUE_MALFORMED);

    if (auto const& issuer = request.at(JS(issuer)); !issuer.is_string() ||
        !ripple::to_issuer(issue.account, issuer.as_string().c_str()))
        return ripple::Unexpected(ripple::rpcAMM_ISSUE_MALFORMED);

    return issue;
}

Result
doAMMInfo(Context const& context)
{
    auto params = context.params;
    std::optional<ripple::AccountID> accountID;
    ripple::Issue issue1{ripple::noIssue()};
    ripple::Issue issue2{ripple::noIssue()};

    // error if asset/asset2 fields don't exist
    if (!params.contains(JS(asset)) || !params.contains(JS(asset2)))
        return Status{ripple::rpcINVALID_PARAMS};

    if (auto const i = getIssue(params.at(JS(asset))); !i)
        return Status{i.error()};
    else
        issue1 = *i;

    if (auto const i = getIssue(params.at(JS(asset2))); !i)
        return Status{i.error()};
    else
        issue2 = *i;

    auto v = ledgerInfoFromRequest(context);
    if (auto status = std::get_if<Status>(&v))
        return *status;

    auto lgrInfo = std::get<ripple::LedgerInfo>(v);
    if (params.contains(JS(account)))
    {
        if (auto const status =
                getOptionalAccount(params, accountID, JS(account));
            status)
        {
            return status;
        }

        auto ammKeylet = ripple::keylet::account(*accountID);
        std::optional<std::vector<unsigned char>> dbResponse =
            context.backend->fetchLedgerObject(
                ammKeylet.key, lgrInfo.seq, context.yield);

        if (!dbResponse)
            return Status{RippledError::rpcACT_NOT_FOUND};

        if (auto const sle = read(ammKeylet, lgrInfo, context);
            !accountID || !sle)
            return Status{ripple::rpcACT_MALFORMED};
    }

    auto ammKeylet = ripple::keylet::amm(issue1, issue2);
    auto const amm = read(ammKeylet, lgrInfo, context);
    if (!amm ||
        !read(
            ripple::keylet::account(amm->getAccountID(ripple::sfAMMAccount)),
            lgrInfo,
            context))
        return Status{RippledError::rpcACT_NOT_FOUND};

    auto const ammAccountID = amm->getAccountID(ripple::sfAMMAccount);
    auto const [asset1Balance, asset2Balance] = getAmmPoolHolds(
        *context.backend,
        lgrInfo.seq,
        ammAccountID,
        issue1,
        issue2,
        context.yield);

    auto const lptAMMBalance = accountID
        ? getAmmLpHolds(
              *context.backend, lgrInfo.seq, *(amm), *accountID, context.yield)
        : (*amm)[ripple::sfLPTokenBalance];

    boost::json::object result;

    result[JS(Amount)] =
        toBoostJson(asset1Balance.getJson(ripple::JsonOptions::none));
    result[JS(Amount2)] =
        toBoostJson(asset2Balance.getJson(ripple::JsonOptions::none));
    result[JS(LPToken)] =
        toBoostJson(lptAMMBalance.getJson(ripple::JsonOptions::none));
    result[JS(TradingFee)] = (*amm)[ripple::sfTradingFee];
    result[JS(AMMAccount)] = to_string(ammAccountID);
    result[JS(ledger_index)] = lgrInfo.seq;

    boost::json::array voteSlots;
    if (amm->isFieldPresent(ripple::sfVoteSlots))
    {
        for (auto const& voteEntry : amm->getFieldArray(ripple::sfVoteSlots))
        {
            boost::json::object vote;
            vote[JS(TradingFee)] = voteEntry[ripple::sfTradingFee];
            vote[JS(VoteWeight)] = voteEntry[ripple::sfVoteWeight];
            voteSlots.push_back(vote);
        }
    }

    if (voteSlots.size() > 0)
        result[JS(VoteSlots)] = voteSlots;

    if (amm->isFieldPresent(ripple::sfAuctionSlot))
    {
        auto const& auctionSlot = static_cast<ripple::STObject const&>(
            amm->peekAtField(ripple::sfAuctionSlot));
        if (auctionSlot.isFieldPresent(ripple::sfAccount))
        {
            boost::json::object auction;
            auto const timeSlot = getAmmAuctionTimeSlot(
                lgrInfo.parentCloseTime.time_since_epoch().count(),
                auctionSlot);
            auction[JS(TimeInterval)] = timeSlot ? *timeSlot : 0;
            auction[JS(Price)] =
                toBoostJson(auctionSlot[ripple::sfPrice].getJson(
                    ripple::JsonOptions::none));
            auction[JS(DiscountedFee)] = auctionSlot[ripple::sfDiscountedFee];
            result[JS(AuctionSlot)] = auction;
        }
    }

    result[JS(AMMID)] = to_string(ammKeylet.key);
    boost::json::object ammResult;
    ammResult[JS(amm)] = result;
    return ammResult;
}

}  // namespace RPC
