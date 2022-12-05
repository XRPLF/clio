#include <log/Logger.h>
#include <rpc/AMMHelpers.h>

#include <ripple/protocol/digest.h>

using namespace clio;

namespace RPC {

std::pair<ripple::STAmount, ripple::STAmount>
getAmmPoolHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& ammAccountID,
    ripple::Issue const& issue1,
    ripple::Issue const& issue2,
    boost::asio::yield_context& yield)
{
    auto const assetInBalance = accountHolds(
        backend,
        sequence,
        ammAccountID,
        issue1.currency,
        issue1.account,
        true,
        yield);
    auto const assetOutBalance = accountHolds(
        backend,
        sequence,
        ammAccountID,
        issue2.currency,
        issue2.account,
        true,
        yield);
    return std::make_pair(assetInBalance, assetOutBalance);
}

ripple::Currency
getAmmLPTCurrency(ripple::Currency const& cur1, ripple::Currency const& cur2)
{
    std::int32_t constexpr AMMCurrencyCode = 0x03;
    auto const [minC, maxC] = std::minmax(cur1, cur2);
    auto const hash = ripple::sha512Half(minC, maxC);
    ripple::Currency currency;
    *currency.begin() = AMMCurrencyCode;
    std::copy(
        hash.begin(), hash.begin() + currency.size() - 1, currency.begin() + 1);
    return currency;
}

ripple::Issue
getAmmLPTIssue(
    ripple::Currency const& cur1,
    ripple::Currency const& cur2,
    ripple::AccountID const& ammAccountID)
{
    return ripple::Issue(getAmmLPTCurrency(cur1, cur2), ammAccountID);
}

ripple::STAmount
getAmmLpHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::Currency const& cur1,
    ripple::Currency const& cur2,
    ripple::AccountID const& ammAccount,
    ripple::AccountID const& lpAccount,
    boost::asio::yield_context& yield)
{
    auto const lptIssue = getAmmLPTIssue(cur1, cur2, ammAccount);
    return accountHolds(
        backend,
        sequence,
        lpAccount,
        lptIssue.currency,
        lptIssue.account,
        true,
        yield);
}

ripple::STAmount
getAmmLpHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::SLE const& ammSle,
    ripple::AccountID const& lpAccount,
    boost::asio::yield_context& yield)
{
    return getAmmLpHolds(
        backend,
        sequence,
        ammSle[ripple::sfAsset].currency,
        ammSle[ripple::sfAsset2].currency,
        ammSle[ripple::sfAMMAccount],
        lpAccount,
        yield);
}

std::optional<std::uint8_t>
getAmmAuctionTimeSlot(
    std::uint64_t current,
    ripple::STObject const& auctionSlot)
{
    using namespace std::chrono;
    std::uint32_t constexpr totalSlotTimeSecs = 24 * 3600;
    std::uint32_t constexpr intervals = 20;
    std::uint32_t constexpr intervalDuration = totalSlotTimeSecs / intervals;
    if (auto const expiration = auctionSlot[~ripple::sfExpiration])
    {
        auto const diff = current - (*expiration - totalSlotTimeSecs);
        if (diff < totalSlotTimeSecs)
            return diff / intervalDuration;
    }
    return std::nullopt;
}

}  // namespace RPC
