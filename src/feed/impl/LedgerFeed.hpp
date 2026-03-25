#pragma once

#include "data/BackendInterface.hpp"
#include "feed/Types.hpp"
#include "feed/impl/SingleFeedBase.hpp"
#include "util/async/AnyExecutionContext.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <xrpl/protocol/Fees.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <memory>
#include <string>

namespace feed::impl {

/**
 * @brief Feed that publishes the ledger info.
 *  Example : {'type': 'ledgerClosed', 'ledger_index': 2647935, 'ledger_hash':
 * '5D022718CD782A82EE10D2147FD90B5F42F26A7E937C870B4FE3CF1086C916AE', 'ledger_time': 756395681,
 * 'fee_base': 10, 'reserve_base': 10000000, 'reserve_inc': 2000000, 'validated_ledgers':
 * '2619127-2647935', 'txn_count': 0, 'network_id': 1}
 */
class LedgerFeed : public SingleFeedBase {
public:
    /**
     * @brief Construct a new Ledger Feed object
     * @param executionCtx The actual publish will be called in the strand of this.
     */
    LedgerFeed(util::async::AnyExecutionContext& executionCtx)
        : SingleFeedBase(executionCtx, "ledger")
    {
    }

    /**
     * @brief Subscribe the ledger feed.
     * @param yield The coroutine yield.
     * @param backend The backend.
     * @param subscriber The subscriber.
     * @param networkID The network ID.
     * @return The information of the latest ledger.
     */
    boost::json::object
    sub(boost::asio::yield_context yield,
        std::shared_ptr<data::BackendInterface const> const& backend,
        SubscriberSharedPtr const& subscriber,
        uint32_t networkID);

    /**
     * @brief Publishes the ledger feed.
     * @param lgrInfo The ledger header.
     * @param fees The fees.
     * @param ledgerRange The ledger range.
     * @param txnCount The transaction count.
     * @param networkID The network ID.
     */
    void
    pub(ripple::LedgerHeader const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        uint32_t txnCount,
        uint32_t networkID);

private:
    static boost::json::object
    makeLedgerPubMessage(
        ripple::LedgerHeader const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        uint32_t txnCount,
        uint32_t networkID
    );
};
}  // namespace feed::impl
