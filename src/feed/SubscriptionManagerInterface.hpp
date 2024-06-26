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

#include "data/Types.hpp"
#include "feed/Types.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/Fees.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <string>
#include <vector>

namespace feed {

/**
 * @brief Interface of subscription manager.
 * A subscription manager is responsible for managing the subscriptions and publishing the feeds.
 */
class SubscriptionManagerInterface {
public:
    virtual ~SubscriptionManagerInterface() = default;

    /**
     * @brief Subscribe to the book changes feed.
     * @param subscriber
     */
    virtual void
    subBookChanges(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Unsubscribe to the book changes feed.
     * @param subscriber
     */
    virtual void
    unsubBookChanges(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Publish the book changes feed.
     * @param lgrInfo The current ledger header.
     * @param transactions The transactions in the current ledger.
     */
    virtual void
    pubBookChanges(ripple::LedgerHeader const& lgrInfo, std::vector<data::TransactionAndMetadata> const& transactions)
        const = 0;

    /**
     * @brief Subscribe to the proposed transactions feed.
     * @param subscriber
     */
    virtual void
    subProposedTransactions(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Unsubscribe to the proposed transactions feed.
     * @param subscriber
     */
    virtual void
    unsubProposedTransactions(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Subscribe to the proposed transactions feed, only receive the feed when particular account is affected.
     * @param account The account to watch.
     * @param subscriber
     */
    virtual void
    subProposedAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Unsubscribe to the proposed transactions feed for particular account.
     * @param account The account to stop watching.
     * @param subscriber
     */
    virtual void
    unsubProposedAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Forward the proposed transactions feed.
     * @param receivedTxJson The proposed transaction json.
     */
    virtual void
    forwardProposedTransaction(boost::json::object const& receivedTxJson) = 0;

    /**
     * @brief Subscribe to the ledger feed.
     * @param yield The coroutine context
     * @param subscriber The subscriber to the ledger feed
     * @return The ledger feed
     */
    virtual boost::json::object
    subLedger(boost::asio::yield_context yield, SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Unsubscribe to the ledger feed.
     * @param subscriber
     */
    virtual void
    unsubLedger(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Publish the ledger feed.
     * @param lgrInfo The ledger header.
     * @param fees The fees.
     * @param ledgerRange The ledger range.
     * @param txnCount The transaction count.
     */
    virtual void
    pubLedger(
        ripple::LedgerHeader const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        std::uint32_t txnCount
    ) const = 0;

    /**
     * @brief Subscribe to the manifest feed.
     * @param subscriber
     */
    virtual void
    subManifest(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Unsubscribe to the manifest feed.
     * @param subscriber
     */
    virtual void
    unsubManifest(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Forward the manifest feed.
     * @param manifestJson The manifest json to forward.
     */
    virtual void
    forwardManifest(boost::json::object const& manifestJson) const = 0;

    /**
     * @brief Subscribe to the validation feed.
     * @param subscriber
     */
    virtual void
    subValidation(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Unsubscribe to the validation feed.
     * @param subscriber
     */
    virtual void
    unsubValidation(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Forward the validation feed.
     * @param validationJson The validation feed json to forward.
     */
    virtual void
    forwardValidation(boost::json::object const& validationJson) const = 0;

    /**
     * @brief Subscribe to the transactions feed.
     * @param subscriber
     */
    virtual void
    subTransactions(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Unsubscribe to the transactions feed.
     * @param subscriber
     */
    virtual void
    unsubTransactions(SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Subscribe to the transactions feed, only receive the feed when particular account is affected.
     * @param account The account to watch.
     * @param subscriber
     */
    virtual void
    subAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Unsubscribe to the transactions feed for particular account.
     * @param account The account to stop watching
     * @param subscriber The subscriber to unsubscribe
     */
    virtual void
    unsubAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Subscribe to the transactions feed, only receive feed when particular order book is affected.
     * @param book The book to watch.
     * @param subscriber
     */
    virtual void
    subBook(ripple::Book const& book, SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Unsubscribe to the transactions feed for particular order book.
     * @param book The book to watch.
     * @param subscriber
     */
    virtual void
    unsubBook(ripple::Book const& book, SubscriberSharedPtr const& subscriber) = 0;

    /**
     * @brief Forward the transactions feed.
     * @param txMeta The transaction and metadata.
     * @param lgrInfo The ledger header.
     */
    virtual void
    pubTransaction(data::TransactionAndMetadata const& txMeta, ripple::LedgerHeader const& lgrInfo) = 0;

    /**
     * @brief Get the number of subscribers.
     * @return The report of the number of subscribers
     */
    virtual boost::json::object
    report() const = 0;
};

}  // namespace feed
