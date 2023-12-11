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

#include "data/BackendInterface.h"
#include "data/Types.h"
#include "feed/Types.h"
#include "feed/impl/BookChangesFeed.h"
#include "feed/impl/ForwardFeed.h"
#include "feed/impl/LedgerFeed.h"
#include "feed/impl/ProposedTransactionFeed.h"
#include "feed/impl/TransactionFeed.h"
#include "util/log/Logger.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/Book.h>
#include <ripple/protocol/Fees.h>
#include <ripple/protocol/LedgerHeader.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace feed {

class SubscriptionManager {
    std::reference_wrapper<boost::asio::io_context> ioContext_;
    std::shared_ptr<data::BackendInterface const> backend_;

    impl::ForwardFeed manifestFeed_;
    impl::ForwardFeed validationsFeed_;
    impl::LedgerFeed ledgerFeed_;
    impl::BookChangesFeed bookChangesFeed_;
    impl::TransactionFeed transactionFeed_;
    impl::ProposedTransactionFeed proposedTransactionFeed_;

public:
    SubscriptionManager(boost::asio::io_context& ioContext, std::shared_ptr<data::BackendInterface> const& backend)
        : ioContext_(ioContext)
        , backend_(backend)
        , manifestFeed_(ioContext, "manifest")
        , validationsFeed_(ioContext, "validations")
        , ledgerFeed_(ioContext)
        , bookChangesFeed_(ioContext)
        , transactionFeed_(ioContext)
        , proposedTransactionFeed_(ioContext)
    {
    }

    /**
     * @brief Subscribe to the book changes feed.
     */
    void
    subBookChanges(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the book changes feed.
     */
    void
    unsubBookChanges(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Publish the book changes feed.
     * @param lgrInfo: The current ledger header.
     * @param transactions: The transactions in the current ledger.
     */
    void
    pubBookChanges(ripple::LedgerHeader const& lgrInfo, std::vector<data::TransactionAndMetadata> const& transactions)
        const;

    /**
     * @brief Subscribe to the proposed transactions feed.
     */
    void
    subProposedTransactions(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the proposed transactions feed.
     */
    void
    unsubProposedTransactions(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Subscribe to the proposed transactions feed, only receive the feed when particular account is affected.
     */
    void
    subProposedAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the proposed transactions feed for particular account.
     */
    void
    unsubProposedAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Forward the proposed transactions feed.
     * @param receivedTxJson: The proposed transaction json.
     */
    void
    forwardProposedTransaction(boost::json::object const& receivedTxJson);

    /**
     * @brief Subscribe to the ledger feed.
     */
    boost::json::object
    subLedger(boost::asio::yield_context yield, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the ledger feed.
     */
    void
    unsubLedger(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Publish the ledger feed.
     * @param lgrInfo: The ledger header.
     * @param fees: The fees.
     * @param ledgerRange: The ledger range.
     * @param txnCount: The transaction count.
     */
    void
    pubLedger(
        ripple::LedgerHeader const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        std::uint32_t txnCount
    ) const;

    /**
     * @brief Subscribe to the manifest feed.
     */
    void
    subManifest(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the manifest feed.
     */
    void
    unsubManifest(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Forward the manifest feed.
     * @param manifestJson: The manifest json to forward.
     */
    void
    forwardManifest(boost::json::object const& manifestJson) const;

    /**
     * @brief Subscribe to the validation feed.
     */
    void
    subValidation(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the validation feed.
     */
    void
    unsubValidation(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Forward the validation feed.
     * @param validationJson: The validation feed json to forward.
     */
    void
    forwardValidation(boost::json::object const& validationJson) const;

    /**
     * @brief Subscribe to the transactions feed.
     * @param apiVersion: The api version of feed to subscribe.
     */
    void
    subTransactions(SubscriberSharedPtr const& subscriber, std::uint32_t apiVersion);

    /**
     * @brief Unsubscribe to the transactions feed.
     */
    void
    unsubTransactions(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Subscribe to the transactions feed, only receive the feed when particular account is affected.
     * @param account: The account to watch.
     * @param apiVersion: The api version of feed to subscribe.
     */
    void
    subAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber, std::uint32_t apiVersion);

    /**
     * @brief Unsubscribe to the transactions feed for particular account.
     * @param subscriber: The subscriber.
     */
    void
    unsubAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Subscribe to the transactions feed, only receive feed when particular order book is affected.
     * @param book: The book to watch.
     * @param apiVersion: The api version of feed to subscribe.
     */
    void
    subBook(ripple::Book const& book, SubscriberSharedPtr const& subscriber, std::uint32_t apiVersion);

    /**
     * @brief Unsubscribe to the transactions feed for particular order book.
     * @param book: The book to watch.
     */
    void
    unsubBook(ripple::Book const& book, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Forward the transactions feed.
     * @param txMeta: The transaction and metadata.
     * @param lgrInfo: The ledger header.
     */
    void
    pubTransaction(data::TransactionAndMetadata const& txMeta, ripple::LedgerHeader const& lgrInfo);

    /**
     * @brief Get the number of subscribers.
     */
    boost::json::object
    report() const
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
 * @brief The help class to run the subscription manager. The container of io_context which is used to publish the
 * feeds.
 */
class SubscriptionManagerRunner {
    boost::asio::io_context ioContext_;
    std::shared_ptr<SubscriptionManager> SubscriptionManager_;
    util::Logger logger_{"Subscriptions"};
    std::optional<boost::asio::io_context::work> work_;
    std::vector<std::thread> workers_;

public:
    SubscriptionManagerRunner(util::Config const& config, std::shared_ptr<data::BackendInterface> const& backend)
        : SubscriptionManager_(std::make_shared<SubscriptionManager>(ioContext_, backend))
    {
        auto numThreads = config.valueOr<uint64_t>("subscription_workers", 1);
        LOG(logger_.info()) << "Starting subscription manager with " << numThreads << " workers";
        workers_.reserve(numThreads);
        work_.emplace(ioContext_);
        for (auto i = numThreads; i > 0; --i)
            workers_.emplace_back([&] { ioContext_.run(); });
    }

    std::shared_ptr<SubscriptionManager>
    get()
    {
        return SubscriptionManager_;
    }

    ~SubscriptionManagerRunner()
    {
        work_.reset();
        for (auto& worker : workers_)
            worker.join();
        SubscriptionManager_.reset();
    }
};
}  // namespace feed
