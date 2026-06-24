#pragma once

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
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

class TransactionFeed {
    // Hold two versions of transaction messages
    struct AllVersionsMsgsType {
        std::string v1;
        std::string v2;
    };

    struct TransactionSlot {
        std::reference_wrapper<TransactionFeed> feed;
        std::weak_ptr<Subscriber> subscriptionContextWeakPtr;

        TransactionSlot(TransactionFeed& feed, SubscriberSharedPtr const& connection)
            : feed(feed), subscriptionContextWeakPtr(connection)
        {
        }

        void
        operator()(std::shared_ptr<AllVersionsMsgsType> const& allVersionMsgs) const;
    };

    util::Logger logger_{"Subscriptions"};

    util::async::AnyStrand strand_;
    std::reference_wrapper<util::prometheus::GaugeInt> subAllCount_;
    std::reference_wrapper<util::prometheus::GaugeInt> subAccountCount_;
    std::reference_wrapper<util::prometheus::GaugeInt> subBookCount_;

    TrackableSignalMap<xrpl::AccountID, Subscriber, std::shared_ptr<AllVersionsMsgsType> const&>
        accountSignal_;
    TrackableSignalMap<xrpl::Book, Subscriber, std::shared_ptr<AllVersionsMsgsType> const&>
        bookSignal_;
    TrackableSignal<Subscriber, std::shared_ptr<AllVersionsMsgsType> const&> signal_;

    // Signals for proposed tx subscribers
    TrackableSignalMap<xrpl::AccountID, Subscriber, std::shared_ptr<AllVersionsMsgsType> const&>
        accountProposedSignal_;
    TrackableSignal<Subscriber, std::shared_ptr<AllVersionsMsgsType> const&> txProposedSignal_;

    std::unordered_set<SubscriberPtr> notified_;  // Used by slots to prevent double notifications
                                                  // if tx contains multiple subscribed accounts

public:
    /**
     * @brief Construct a new Transaction Feed object.
     * @param executionCtx The actual publish will be called in the strand of this.
     */
    TransactionFeed(util::async::AnyExecutionContext& executionCtx)
        : strand_(executionCtx.makeStrand())
        , subAllCount_(getSubscriptionsGaugeInt("tx"))
        , subAccountCount_(getSubscriptionsGaugeInt("account"))
        , subBookCount_(getSubscriptionsGaugeInt("book"))
    {
    }

    /**
     * @brief Move constructor is deleted because TransactionSlot takes TransactionFeed by reference
     */
    TransactionFeed(TransactionFeed&&) = delete;

    /**
     * @brief Subscribe to the transaction feed.
     * @param subscriber
     */
    void
    sub(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Subscribe to the transaction feed, only receive the feed when particular account is
     * affected.
     * @param subscriber
     * @param account The account to watch.
     */
    void
    sub(xrpl::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Subscribe to the transaction feed, only receive the feed when particular order book is
     * affected.
     * @param subscriber
     * @param book The order book to watch.
     */
    void
    sub(xrpl::Book const& book, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Subscribe to the transaction feed for proposed transaction stream.
     * @param subscriber
     */
    void
    subProposed(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Subscribe to the transaction feed for proposed account, only receive the feed when
     * particular account is affected.
     * @param subscriber
     * @param account The account to watch.
     */
    void
    subProposed(xrpl::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the transaction feed.
     * @param subscriber
     */
    void
    unsub(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the transaction for particular account.
     * @param subscriber
     * @param account The account to unsubscribe.
     */
    void
    unsub(xrpl::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the transaction feed for proposed transaction stream.
     * @param subscriber
     */
    void
    unsubProposed(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the transaction for particular proposed account.
     * @param subscriber
     * @param account The account to unsubscribe.
     */
    void
    unsubProposed(xrpl::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the transaction feed for particular order book.
     * @param subscriber
     * @param book The book to unsubscribe.
     */
    void
    unsub(xrpl::Book const& book, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Publishes the transaction feed.
     * @param txMeta The transaction and metadata.
     * @param lgrInfo The ledger header.
     * @param backend The backend.
     * @param networkID The network ID.
     */
    void
    pub(data::TransactionAndMetadata const& txMeta,
        xrpl::LedgerHeader const& lgrInfo,
        std::shared_ptr<data::BackendInterface const> const& backend,
        std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter,
        uint32_t networkID);

    /**
     * @brief Get the number of subscribers of the transaction feed.
     */
    std::uint64_t
    transactionSubCount() const;

    /**
     * @brief Get the number of accounts subscribers.
     */
    std::uint64_t
    accountSubCount() const;

    /**
     * @brief Get the number of books subscribers.
     */
    std::uint64_t
    bookSubCount() const;

private:
    void
    unsubInternal(SubscriberPtr subscriber);

    void
    unsubInternal(xrpl::AccountID const& account, SubscriberPtr subscriber);

    void
    unsubProposedInternal(SubscriberPtr subscriber);

    void
    unsubProposedInternal(xrpl::AccountID const& account, SubscriberPtr subscriber);

    void
    unsubInternal(xrpl::Book const& book, SubscriberPtr subscriber);
};
}  // namespace feed::impl
