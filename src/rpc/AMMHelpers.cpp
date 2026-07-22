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

#include <cstdint>
#include <utility>

namespace rpc {

std::pair<xrpl::STAmount, xrpl::STAmount>
getAmmPoolHolds(
    BackendInterface const& backend,
    data::AmendmentCenterInterface const& amendmentCenter,
    std::uint32_t sequence,
    xrpl::AccountID const& ammAccountID,
    xrpl::Issue const& issue1,
    xrpl::Issue const& issue2,
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

xrpl::STAmount
getAmmLpHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::Issue const& iss1,
    xrpl::Issue const& iss2,
    xrpl::AccountID const& ammAccount,
    xrpl::AccountID const& lpAccount,
    boost::asio::yield_context yield
)
{
    auto const lptCurrency = ammLPTCurrency(iss1, iss2);

    // not using accountHolds because we don't need to check if the associated tokens of the LP are
    // frozen
    return ammAccountHolds(backend, sequence, lpAccount, lptCurrency, ammAccount, true, yield);
}

xrpl::STAmount
getAmmLpHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::SLE const& ammSle,
    xrpl::AccountID const& lpAccount,
    boost::asio::yield_context yield
)
{
    return getAmmLpHolds(
        backend,
        sequence,
        ammSle[xrpl::sfAsset].get<xrpl::Issue>(),
        ammSle[xrpl::sfAsset2].get<xrpl::Issue>(),
        ammSle[xrpl::sfAccount],
        lpAccount,
        yield
    );
}

}  // namespace rpc
