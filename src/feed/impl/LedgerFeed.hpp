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

#pragma once

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
#include <string>

namespace feed::impl {

/**
 * @brief Feed that publishes the ledger info.
 *  Example : {'type': 'ledgerClosed', 'ledger_index': 2647935, 'ledger_hash':
 * '5D022718CD782A82EE10D2147FD90B5F42F26A7E937C870B4FE3CF1086C916AE', 'ledger_time': 756395681, 'fee_base': 10,
 * 'reserve_base': 10000000, 'reserve_inc': 2000000, 'validated_ledgers': '2619127-2647935', 'txn_count': 0}
 *
 * @tparam ExecutionContext The type of the execution context.
 */

template <class ExecutionContext>
class LedgerFeed : public SingleFeedBase<ExecutionContext> {
public:
    /**
     * @brief Construct a new Ledger Feed object
     *
     * @param executorContext The actual publish will be called in the strand of this.
     */
    LedgerFeed(ExecutionContext& executorContext) : SingleFeedBase<ExecutionContext>(executorContext, "ledger")
    {
    }

    /**
     * @brief Subscribe the ledger feed.
     *
     * @param yield The coroutine yield.
     * @param backend The backend.
     * @param subscriber
     * @return The information of the latest ledger.
     */
    boost::json::object
    sub(boost::asio::yield_context yield,
        std::shared_ptr<data::BackendInterface const> const& backend,
        SubscriberSharedPtr const& subscriber)
    {
        SingleFeedBase<ExecutionContext>::sub(subscriber);

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

    /**
     * @brief Publishes the ledger feed.
     *
     * @param lgrInfo The ledger header.
     * @param fees The fees.
     * @param ledgerRange The ledger range.
     * @param txnCount The transaction count.
     */
    void
    pub(ripple::LedgerHeader const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        std::uint32_t txnCount) const
    {
        SingleFeedBase<ExecutionContext>::pub(
            boost::json::serialize(makeLedgerPubMessage(lgrInfo, fees, ledgerRange, txnCount))
        );
    }

private:
    static boost::json::object
    makeLedgerPubMessage(
        ripple::LedgerHeader const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        std::uint32_t txnCount
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
};
}  // namespace feed::impl
