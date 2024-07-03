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

#include "feed/impl/LedgerFeed.hpp"

#include "data/BackendInterface.hpp"
#include "feed/Types.hpp"
#include "feed/impl/SingleFeedBase.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/Assert.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Fees.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace feed::impl {

boost::json::object
LedgerFeed::makeLedgerPubMessage(
    ripple::LedgerHeader const& lgrInfo,
    ripple::Fees const& fees,
    std::string const& ledgerRange,
    std::uint32_t const txnCount
)
{
    boost::json::object pubMsg;
    pubMsg["type"] = "ledgerClosed";
    pubMsg["ledger_index"] = lgrInfo.seq;
    pubMsg["ledger_hash"] = to_string(lgrInfo.hash);
    pubMsg["ledger_time"] = lgrInfo.closeTime.time_since_epoch().count();
    pubMsg["fee_base"] = rpc::toBoostJson(fees.base.jsonClipped());
    pubMsg["reserve_base"] = rpc::toBoostJson(fees.reserve.jsonClipped());
    pubMsg["reserve_inc"] = rpc::toBoostJson(fees.increment.jsonClipped());
    pubMsg["validated_ledgers"] = ledgerRange;
    pubMsg["txn_count"] = txnCount;
    return pubMsg;
}

boost::json::object
LedgerFeed::sub(
    boost::asio::yield_context yield,
    std::shared_ptr<data::BackendInterface const> const& backend,
    SubscriberSharedPtr const& subscriber
)
{
    SingleFeedBase::sub(subscriber);

    // For ledger stream, we need to send the last closed ledger info as response
    auto const ledgerRange = backend->fetchLedgerRange();
    ASSERT(ledgerRange.has_value(), "Ledger range must be valid");

    auto const lgrInfo = backend->fetchLedgerBySequence(ledgerRange->maxSequence, yield);
    ASSERT(lgrInfo.has_value(), "Ledger must be valid");

    auto const fees = backend->fetchFees(lgrInfo->seq, yield);
    ASSERT(fees.has_value(), "Fees must be valid");

    auto const range = std::to_string(ledgerRange->minSequence) + "-" + std::to_string(ledgerRange->maxSequence);

    auto pubMsg = makeLedgerPubMessage(*lgrInfo, *fees, range, 0);
    pubMsg.erase("txn_count");
    pubMsg.erase("type");

    return pubMsg;
}

void
LedgerFeed::pub(
    ripple::LedgerHeader const& lgrInfo,
    ripple::Fees const& fees,
    std::string const& ledgerRange,
    std::uint32_t const txnCount
) const
{
    SingleFeedBase::pub(boost::json::serialize(makeLedgerPubMessage(lgrInfo, fees, ledgerRange, txnCount)));
}
}  // namespace feed::impl
