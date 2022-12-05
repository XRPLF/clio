#pragma once

#include <backend/BackendInterface.h>
#include <rpc/RPC.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

std::pair<ripple::STAmount, ripple::STAmount>
getAmmPoolHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& ammAccountID,
    ripple::Issue const& issue1,
    ripple::Issue const& issue2,
    boost::asio::yield_context& yield);

ripple::Currency
getAmmLPTCurrency(ripple::Currency const& cur1, ripple::Currency const& cur2);

ripple::Issue
getAmmLPTIssue(
    ripple::Currency const& cur1,
    ripple::Currency const& cur2,
    ripple::AccountID const& ammAccountID);

ripple::STAmount
getAmmLpHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::Currency const& cur1,
    ripple::Currency const& cur2,
    ripple::AccountID const& ammAccount,
    ripple::AccountID const& lpAccount,
    boost::asio::yield_context& yield);

ripple::STAmount
getAmmLpHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::SLE const& ammSle,
    ripple::AccountID const& lpAccount,
    boost::asio::yield_context& yield);

std::optional<std::uint8_t>
getAmmAuctionTimeSlot(
    std::uint64_t current,
    ripple::STObject const& auctionSlot);

}  // namespace RPC
