#pragma once

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"

#include <boost/asio/spawn.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/UintTypes.h>

#include <cstdint>
#include <utility>

namespace rpc {

/**
 * @brief getAmmPoolHolds returns the balances of the amm asset pair
 *
 * @param backend The backend to use
 * @param amendmentCenter The amendmentCenter to use
 * @param sequence The sequence number to use
 * @param ammAccountID The amm account
 * @param issue1 The first issue
 * @param issue2 The second issue
 * @param freezeHandling Whether to return zeroes for frozen accounts
 * @param yield The coroutine context
 * @return The balances of the amm asset pair
 */
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
);

/**
 * @brief getAmmLpHolds returns the liquidity provider token balance
 *
 * @param backend The backend to use
 * @param sequence The sequence number to use
 * @param iss1 The first issue
 * @param iss2 The second issue
 * @param ammAccount The amm account
 * @param lpAccount The lp account
 * @param yield The coroutine context
 * @return The lp token balance
 */
xrpl::STAmount
getAmmLpHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::Issue const& iss1,
    xrpl::Issue const& iss2,
    xrpl::AccountID const& ammAccount,
    xrpl::AccountID const& lpAccount,
    boost::asio::yield_context yield
);

/**
 * @brief getAmmLpHolds returns the liquidity provider token balance
 *
 * @param backend The backend to use
 * @param sequence The sequence number to use
 * @param ammSle The amm ledger entry
 * @param lpAccount The lp account
 * @param yield The coroutine context
 * @return The lp token balance
 */
xrpl::STAmount
getAmmLpHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    xrpl::SLE const& ammSle,
    xrpl::AccountID const& lpAccount,
    boost::asio::yield_context yield
);

}  // namespace rpc
