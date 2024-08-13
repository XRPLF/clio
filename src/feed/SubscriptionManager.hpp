//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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
#include "data/Types.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "feed/Types.hpp"
#include "feed/impl/BookChangesFeed.hpp"
#include "feed/impl/ForwardFeed.hpp"
#include "feed/impl/LedgerFeed.hpp"
#include "feed/impl/ProposedTransactionFeed.hpp"
#include "feed/impl/TransactionFeed.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Fees.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/**
 * @brief This namespace implements everything related to subscriptions.
 *
 * The subscription manager is responsible for managing the subscriptions and publishing the feeds.
 */
namespace feed {

/**
 * @brief A subscription manager is responsible for managing the subscriptions and publishing the feeds
 * @tparam ExecutionContext The type of the execution context, which must be some kind of BasicExecutionContext
 */
template <class ExecutionContext>
    requires util::IsInstanceOfV<util::async::BasicExecutionContext, ExecutionContext>
class SubscriptionManager : public SubscriptionManagerInterface {
    std::shared_ptr<data::BackendInterface const> backend_;
    ExecutionContext ctx_;
    impl::ForwardFeed<ExecutionContext> manifestFeed_;
    impl::ForwardFeed<ExecutionContext> validationsFeed_;
    impl::LedgerFeed<ExecutionContext> ledgerFeed_;
    impl::BookChangesFeed<ExecutionContext> bookChangesFeed_;
    impl::TransactionFeed<ExecutionContext> transactionFeed_;
    impl::ProposedTransactionFeed<ExecutionContext> proposedTransactionFeed_;

public:
    /**
     * @brief Construct a new Subscription Manager object
     *
     * @param backend The backend to use
     * @param numThreads The number of threads to use to publish the feeds, 1 by default
     */
    SubscriptionManager(std::shared_ptr<data::BackendInterface const> const& backend, std::uint32_t numThreads = 1)
        : backend_(backend)
        , ctx_(numThreads)
        , manifestFeed_(ctx_, "manifest")
        , validationsFeed_(ctx_, "validations")
        , ledgerFeed_(ctx_)
        , bookChangesFeed_(ctx_)
        , transactionFeed_(ctx_)
        , proposedTransactionFeed_(ctx_)
    {
    }

    /**
     * @brief Destructor of the SubscriptionManager object. It will block until all the executor threads are stopped.
     */
    ~SubscriptionManager() override
    {
        ctx_.stop();
        ctx_.join();
    }

    /**
     * @brief Subscribe to the book changes feed.
     * @param subscriber
     */
    void
    subBookChanges(SubscriberSharedPtr const& subscriber) final
    {
        bookChangesFeed_.sub(subscriber);
    }

    /**
     * @brief Unsubscribe to the book changes feed.
     * @param subscriber
     */
    void
    unsubBookChanges(SubscriberSharedPtr const& subscriber) final
    {
        bookChangesFeed_.unsub(subscriber);
    }

    /**
     * @brief Publish the book changes feed.
     * @param lgrInfo The current ledger header.
     * @param transactions The transactions in the current ledger.
     */
    void
    pubBookChanges(ripple::LedgerHeader const& lgrInfo, std::vector<data::TransactionAndMetadata> const& transactions)
        const final
    {
        bookChangesFeed_.pub(lgrInfo, transactions);
    }

    /**
     * @brief Subscribe to the proposed transactions feed.
     * @param subscriber
     */
    void
    subProposedTransactions(SubscriberSharedPtr const& subscriber) final
    {
        proposedTransactionFeed_.sub(subscriber);
        // proposed_transactions subscribers not only receive the transaction json when it is proposed, but also the
        // transaction json when it is validated. So the subscriber also subscribes to the transaction feed.
        transactionFeed_.subProposed(subscriber);
    }

    /**
     * @brief Unsubscribe to the proposed transactions feed.
     * @param subscriber
     */
    void
    unsubProposedTransactions(SubscriberSharedPtr const& subscriber) final
    {
        proposedTransactionFeed_.unsub(subscriber);
        transactionFeed_.unsubProposed(subscriber);
    }

    /**
     * @brief Subscribe to the proposed transactions feed, only receive the feed when particular account is affected.
     * @param account The account to watch.
     * @param subscriber
     */
    void
    subProposedAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) final
    {
        proposedTransactionFeed_.sub(account, subscriber);
        // Same as proposed_transactions subscribers, proposed_account subscribers also subscribe to the transaction
        // feed to receive validated transaction feed. TransactionFeed class will filter out the sessions that have been
        // sent to.
        transactionFeed_.subProposed(account, subscriber);
    }

    /**
     * @brief Unsubscribe to the proposed transactions feed for particular account.
     * @param account The account to stop watching.
     * @param subscriber
     */
    void
    unsubProposedAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) final
    {
        proposedTransactionFeed_.unsub(account, subscriber);
        transactionFeed_.unsubProposed(account, subscriber);
    }

    /**
     * @brief Forward the proposed transactions feed.
     * @param receivedTxJson The proposed transaction json.
     */
    void
    forwardProposedTransaction(boost::json::object const& receivedTxJson) final
    {
        proposedTransactionFeed_.pub(receivedTxJson);
    }

    /**
     * @brief Subscribe to the ledger feed.
     * @param yield The coroutine context
     * @param subscriber The subscriber to the ledger feed
     * @return The ledger feed
     */
    boost::json::object
    subLedger(boost::asio::yield_context yield, SubscriberSharedPtr const& subscriber) final
    {
        return ledgerFeed_.sub(yield, backend_, subscriber);
    }

    /**
     * @brief Unsubscribe to the ledger feed.
     * @param subscriber
     */
    void
    unsubLedger(SubscriberSharedPtr const& subscriber) final
    {
        ledgerFeed_.unsub(subscriber);
    }

    /**
     * @brief Publish the ledger feed.
     * @param lgrInfo The ledger header.
     * @param fees The fees.
     * @param ledgerRange The ledger range.
     * @param txnCount The transaction count.
     */
    void
    pubLedger(
        ripple::LedgerHeader const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        std::uint32_t txnCount
    ) const final
    {
        ledgerFeed_.pub(lgrInfo, fees, ledgerRange, txnCount);
    }

    /**
     * @brief Subscribe to the manifest feed.
     * @param subscriber
     */
    void
    subManifest(SubscriberSharedPtr const& subscriber) final
    {
        manifestFeed_.sub(subscriber);
    }

    /**
     * @brief Unsubscribe to the manifest feed.
     * @param subscriber
     */
    void
    unsubManifest(SubscriberSharedPtr const& subscriber) final
    {
        manifestFeed_.unsub(subscriber);
    }

    /**
     * @brief Forward the manifest feed.
     * @param manifestJson The manifest json to forward.
     */
    void
    forwardManifest(boost::json::object const& manifestJson) const final
    {
        manifestFeed_.pub(manifestJson);
    }

    /**
     * @brief Subscribe to the validation feed.
     * @param subscriber
     */
    void
    subValidation(SubscriberSharedPtr const& subscriber) final
    {
        validationsFeed_.sub(subscriber);
    }

    /**
     * @brief Unsubscribe to the validation feed.
     * @param subscriber
     */
    void
    unsubValidation(SubscriberSharedPtr const& subscriber) final
    {
        validationsFeed_.unsub(subscriber);
    }

    /**
     * @brief Forward the validation feed.
     * @param validationJson The validation feed json to forward.
     */
    void
    forwardValidation(boost::json::object const& validationJson) const final
    {
        validationsFeed_.pub(validationJson);
    }

    /**
     * @brief Subscribe to the transactions feed.
     * @param subscriber
     */
    void
    subTransactions(SubscriberSharedPtr const& subscriber) final
    {
        transactionFeed_.sub(subscriber);
    }

    /**
     * @brief Unsubscribe to the transactions feed.
     * @param subscriber
     */
    void
    unsubTransactions(SubscriberSharedPtr const& subscriber) final
    {
        transactionFeed_.unsub(subscriber);
    }

    /**
     * @brief Subscribe to the transactions feed, only receive the feed when particular account is affected.
     * @param account The account to watch.
     * @param subscriber
     */
    void
    subAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) final
    {
        transactionFeed_.sub(account, subscriber);
    }

    /**
     * @brief Unsubscribe to the transactions feed for particular account.
     * @param account The account to stop watching
     * @param subscriber The subscriber to unsubscribe
     */
    void
    unsubAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) final
    {
        transactionFeed_.unsub(account, subscriber);
    }

    /**
     * @brief Subscribe to the transactions feed, only receive feed when particular order book is affected.
     * @param book The book to watch.
     * @param subscriber
     */
    void
    subBook(ripple::Book const& book, SubscriberSharedPtr const& subscriber) final
    {
        transactionFeed_.sub(book, subscriber);
    }

    /**
     * @brief Unsubscribe to the transactions feed for particular order book.
     * @param book The book to watch.
     * @param subscriber
     */
    void
    unsubBook(ripple::Book const& book, SubscriberSharedPtr const& subscriber) final
    {
        transactionFeed_.unsub(book, subscriber);
    }

    /**
     * @brief Forward the transactions feed.
     * @param txMeta The transaction and metadata.
     * @param lgrInfo The ledger header.
     */
    void
    pubTransaction(data::TransactionAndMetadata const& txMeta, ripple::LedgerHeader const& lgrInfo) final
    {
        transactionFeed_.pub(txMeta, lgrInfo, backend_);
    }

    /**
     * @brief Get the number of subscribers.
     *
     * @return The report of the number of subscribers
     */
    boost::json::object
    report() const final
    {
        return {
            {"ledger", ledgerFeed_.count()},
            {"transactions", transactionFeed_.transactionSubCount()},
            {"transactions_proposed", proposedTransactionFeed_.transactionSubcount()},
            {"manifests", manifestFeed_.count()},
            {"validations", validationsFeed_.count()},
            {"account", transactionFeed_.accountSubCount()},
            {"accounts_proposed", proposedTransactionFeed_.accountSubCount()},
            {"books", transactionFeed_.bookSubCount()},
            {"book_changes", bookChangesFeed_.count()},
        };
    }
};

/**
 * @brief The help class to run the subscription manager with a pool executor.
 */
class SubscriptionManagerRunner {
    std::uint64_t workersNum_;
    using ExecutionCtx = util::async::PoolExecutionContext;
    std::shared_ptr<SubscriptionManager<ExecutionCtx>> subscriptionManager_;
    util::Logger logger_{"Subscriptions"};

public:
    /**
     * @brief Construct a new Subscription Manager Runner object
     *
     * @param config The configuration
     * @param backend The backend to use
     */
    SubscriptionManagerRunner(util::Config const& config, std::shared_ptr<data::BackendInterface> const& backend)
        : workersNum_(config.valueOr<std::uint64_t>("subscription_workers", 1))
        , subscriptionManager_(std::make_shared<SubscriptionManager<ExecutionCtx>>(backend, workersNum_))
    {
        LOG(logger_.info()) << "Starting subscription manager with " << workersNum_ << " workers";
    }

    /**
     * @brief Get the subscription manager
     *
     * @return The subscription manager
     */
    auto
    getManager()
    {
        return subscriptionManager_;
    }
};
}  // namespace feed
