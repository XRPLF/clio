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

#include "rpc/AMMHelpers.h"

#include "data/BackendInterface.h"
#include "rpc/RPCHelpers.h"

#include <boost/asio/spawn.hpp>
#include <ripple/protocol/AMMCore.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/Issue.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/UintTypes.h>

#include <cstdint>
#include <utility>

namespace rpc {

std::pair<ripple::STAmount, ripple::STAmount>
getAmmPoolHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& ammAccountID,
    ripple::Issue const& issue1,
    ripple::Issue const& issue2,
    bool freezeHandling,
    boost::asio::yield_context yield
)
{
    auto const assetInBalance =
        accountHolds(backend, sequence, ammAccountID, issue1.asset(), issue1.account(), freezeHandling, yield);
    auto const assetOutBalance =
        accountHolds(backend, sequence, ammAccountID, issue2.asset(), issue2.account(), freezeHandling, yield);
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
    return accountHolds(backend, sequence, lpAccount, lptCurrency, ammAccount, true, yield);
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
        ammSle[ripple::sfAsset].asset(),
        ammSle[ripple::sfAsset2].asset(),
        ammSle[ripple::sfAccount],
        lpAccount,
        yield
    );
}

}  // namespace rpc
