#include "rpc/AMMHelpers.hpp"

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "rpc/RPCHelpers.hpp"

#include <boost/asio/spawn.hpp>
#include <xrpl/protocol/AMMCore.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <utility>

namespace rpc {

std::pair<ripple::STAmount, ripple::STAmount>
getAmmPoolHolds(
    BackendInterface const& backend,
    data::AmendmentCenterInterface const& amendmentCenter,
    std::uint32_t sequence,
    ripple::AccountID const& ammAccountID,
    ripple::Issue const& issue1,
    ripple::Issue const& issue2,
    bool freezeHandling,
    boost::asio::yield_context yield
)
{
    auto const assetInBalance = accountHolds(
        backend,
        amendmentCenter,
        sequence,
        ammAccountID,
        issue1.currency,
        issue1.account,
        freezeHandling,
        yield
    );
    auto const assetOutBalance = accountHolds(
        backend,
        amendmentCenter,
        sequence,
        ammAccountID,
        issue2.currency,
        issue2.account,
        freezeHandling,
        yield
    );
    return std::make_pair(assetInBalance, assetOutBalance);
}

ripple::STAmount
getAmmLpHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::Currency const& cur1,
    ripple::Currency const& cur2,
    ripple::AccountID const& ammAccount,
    ripple::AccountID const& lpAccount,
    boost::asio::yield_context yield
)
{
    auto const lptCurrency = ammLPTCurrency(cur1, cur2);

    // not using accountHolds because we don't need to check if the associated tokens of the LP are
    // frozen
    return ammAccountHolds(backend, sequence, lpAccount, lptCurrency, ammAccount, true, yield);
}

ripple::STAmount
getAmmLpHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::SLE const& ammSle,
    ripple::AccountID const& lpAccount,
    boost::asio::yield_context yield
)
{
    return getAmmLpHolds(
        backend,
        sequence,
        ammSle[ripple::sfAsset].get<ripple::Issue>().currency,
        ammSle[ripple::sfAsset2].get<ripple::Issue>().currency,
        ammSle[ripple::sfAccount],
        lpAccount,
        yield
    );
}

}  // namespace rpc
