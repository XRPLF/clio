#pragma once

#include "feed/Types.hpp"
#include "feed/impl/TrackableSignal.hpp"
#include "feed/impl/TrackableSignalMap.hpp"
#include "feed/impl/Util.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyStrand.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Gauge.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json/object.hpp>
#include <fmt/format.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>

namespace feed::impl {

/**
 * @brief Feed that publishes the Proposed Transactions.
 */
class ProposedTransactionFeed {
    // Hold two versions of transaction messages
    struct AllVersionMsgsType {
        std::string v1;
        std::string v2;
    };

    using AllVersionsMsgsPtrType = std::shared_ptr<AllVersionMsgsType>;

    class ProposedTransactionSlot {
        std::reference_wrapper<ProposedTransactionFeed> feed_;
        std::weak_ptr<Subscriber> subscriptionContextWeakPtr_;

    public:
        ProposedTransactionSlot(
            ProposedTransactionFeed& feed,
            SubscriberSharedPtr const& connection
        )
            : feed_(feed), subscriptionContextWeakPtr_(connection)
        {
        }

        void
        operator()(AllVersionsMsgsPtrType const& allVersionMsgs) const;
    };

    util::Logger logger_{"Subscriptions"};

    std::unordered_set<SubscriberPtr> notified_;  // Used by slots to prevent double notifications
                                                  // if tx contains multiple subscribed accounts
    util::async::AnyStrand strand_;
    std::reference_wrapper<util::prometheus::GaugeInt> subAllCount_;
    std::reference_wrapper<util::prometheus::GaugeInt> subAccountCount_;

    TrackableSignalMap<ripple::AccountID, Subscriber, AllVersionsMsgsPtrType> accountSignal_;
    TrackableSignal<Subscriber, AllVersionsMsgsPtrType> signal_;

public:
    /**
     * @brief Move constructor is deleted because ProposedTransactionSlot takes
     * ProposedTransactionFeed by reference.
     */
    ProposedTransactionFeed(ProposedTransactionFeed&&) = delete;

    /**
     * @brief Construct a Proposed Transaction Feed object.
     * @param executionCtx The actual publish will be called in the strand of this.
     */
    ProposedTransactionFeed(util::async::AnyExecutionContext& executionCtx)
        : strand_(executionCtx.makeStrand())
        , subAllCount_(getSubscriptionsGaugeInt("tx_proposed"))
        , subAccountCount_(getSubscriptionsGaugeInt("account_proposed"))

    {
    }

    /**
     * @brief Subscribe to the proposed transaction feed.
     * @param subscriber
     */
    void
    sub(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Subscribe to the proposed transaction feed, only receive the feed when particular
     * account is affected.
     * @param subscriber
     * @param account The account to watch.
     */
    void
    sub(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the proposed transaction feed.
     * @param subscriber
     */
    void
    unsub(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the proposed transaction feed for particular account.
     * @param subscriber
     * @param account The account to unsubscribe.
     */
    void
    unsub(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Publishes the proposed transaction feed.
     * @param receivedTxJson The proposed transaction json.
     */
    void
    pub(boost::json::object const& receivedTxJson);

    /**
     * @brief Get the number of subscribers of the proposed transaction feed.
     */
    std::uint64_t
    transactionSubcount() const;

    /**
     * @brief Get the number of accounts subscribers.
     */
    std::uint64_t
    accountSubCount() const;

private:
    void
    unsubInternal(SubscriberPtr subscriber);

    void
    unsubInternal(ripple::AccountID const& account, SubscriberPtr subscriber);
};
}  // namespace feed::impl
