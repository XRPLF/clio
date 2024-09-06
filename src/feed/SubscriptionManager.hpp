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
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Fees.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief This namespace implements everything related to subscriptions.
 *
 * The subscription manager is responsible for managing the subscriptions and publishing the feeds.
 */
namespace feed {

/**
 * @brief A subscription manager is responsible for managing the subscriptions and publishing the feeds
 */
class SubscriptionManager : public SubscriptionManagerInterface {
    std::shared_ptr<data::BackendInterface const> backend_;
    util::async::AnyExecutionContext ctx_;
    impl::ForwardFeed manifestFeed_;
    impl::ForwardFeed validationsFeed_;
    impl::LedgerFeed ledgerFeed_;
    impl::BookChangesFeed bookChangesFeed_;
    impl::TransactionFeed transactionFeed_;
    impl::ProposedTransactionFeed proposedTransactionFeed_;

public:
    /**
     * @brief Factory function to create a new SubscriptionManager with a PoolExecutionContext.
     *
     * @param config The configuration to use
     * @param backend The backend to use
     * @return A shared pointer to a new instance of SubscriptionManager
     */
    static std::shared_ptr<SubscriptionManager>
    make_SubscriptionManager(util::Config const& config, std::shared_ptr<data::BackendInterface const> const& backend)
    {
        auto const workersNum = config.valueOr<std::uint64_t>("subscription_workers", 1);

        util::Logger logger_{"Subscriptions"};
        LOG(logger_.info()) << "Starting subscription manager with " << workersNum << " workers";

        return std::make_shared<feed::SubscriptionManager>(util::async::PoolExecutionContext(workersNum), backend);
    }

    /**
     * @brief Construct a new Subscription Manager object
     *
     * @param executor The executor to use to publish the feeds
     * @param backend The backend to use
     */
    SubscriptionManager(
        util::async::AnyExecutionContext&& executor,
        std::shared_ptr<data::BackendInterface const> const& backend
    )
        : backend_(backend)
        , ctx_(std::move(executor))
        , manifestFeed_(ctx_, "manifest")
        , validationsFeed_(ctx_, "validations")
        , ledgerFeed_(ctx_)
        , bookChangesFeed_(ctx_)
        , transactionFeed_(ctx_)
        , proposedTransactionFeed_(ctx_)
    {
    }

    /**
     * @brief Destructor of the SubscriptionManager object. It will block until all running jobs finished.
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
    subBookChanges(SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Unsubscribe to the book changes feed.
     * @param subscriber
     */
    void
    unsubBookChanges(SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Publish the book changes feed.
     * @param lgrInfo The current ledger header.
     * @param transactions The transactions in the current ledger.
     */
    void
    pubBookChanges(ripple::LedgerHeader const& lgrInfo, std::vector<data::TransactionAndMetadata> const& transactions)
        const final;

    /**
     * @brief Subscribe to the proposed transactions feed.
     * @param subscriber
     */
    void
    subProposedTransactions(SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Unsubscribe to the proposed transactions feed.
     * @param subscriber
     */
    void
    unsubProposedTransactions(SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Subscribe to the proposed transactions feed, only receive the feed when particular account is affected.
     * @param account The account to watch.
     * @param subscriber
     */
    void
    subProposedAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Unsubscribe to the proposed transactions feed for particular account.
     * @param account The account to stop watching.
     * @param subscriber
     */
    void
    unsubProposedAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Forward the proposed transactions feed.
     * @param receivedTxJson The proposed transaction json.
     */
    void
    forwardProposedTransaction(boost::json::object const& receivedTxJson) final;

    /**
     * @brief Subscribe to the ledger feed.
     * @param yield The coroutine context
     * @param subscriber The subscriber to the ledger feed
     * @return The ledger feed
     */
    boost::json::object
    subLedger(boost::asio::yield_context yield, SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Unsubscribe to the ledger feed.
     * @param subscriber
     */
    void
    unsubLedger(SubscriberSharedPtr const& subscriber) final;

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
    ) const final;

    /**
     * @brief Subscribe to the manifest feed.
     * @param subscriber
     */
    void
    subManifest(SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Unsubscribe to the manifest feed.
     * @param subscriber
     */
    void
    unsubManifest(SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Forward the manifest feed.
     * @param manifestJson The manifest json to forward.
     */
    void
    forwardManifest(boost::json::object const& manifestJson) const final;

    /**
     * @brief Subscribe to the validation feed.
     * @param subscriber
     */
    void
    subValidation(SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Unsubscribe to the validation feed.
     * @param subscriber
     */
    void
    unsubValidation(SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Forward the validation feed.
     * @param validationJson The validation feed json to forward.
     */
    void
    forwardValidation(boost::json::object const& validationJson) const final;

    /**
     * @brief Subscribe to the transactions feed.
     * @param subscriber
     */
    void
    subTransactions(SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Unsubscribe to the transactions feed.
     * @param subscriber
     */
    void
    unsubTransactions(SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Subscribe to the transactions feed, only receive the feed when particular account is affected.
     * @param account The account to watch.
     * @param subscriber
     */
    void
    subAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Unsubscribe to the transactions feed for particular account.
     * @param account The account to stop watching
     * @param subscriber The subscriber to unsubscribe
     */
    void
    unsubAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Subscribe to the transactions feed, only receive feed when particular order book is affected.
     * @param book The book to watch.
     * @param subscriber
     */
    void
    subBook(ripple::Book const& book, SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Unsubscribe to the transactions feed for particular order book.
     * @param book The book to watch.
     * @param subscriber
     */
    void
    unsubBook(ripple::Book const& book, SubscriberSharedPtr const& subscriber) final;

    /**
     * @brief Forward the transactions feed.
     * @param txMeta The transaction and metadata.
     * @param lgrInfo The ledger header.
     */
    void
    pubTransaction(data::TransactionAndMetadata const& txMeta, ripple::LedgerHeader const& lgrInfo) final;

    /**
     * @brief Get the number of subscribers.
     *
     * @return The report of the number of subscribers
     */
    boost::json::object
    report() const final;
};

}  // namespace feed
