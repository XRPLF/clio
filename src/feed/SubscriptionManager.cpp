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

#include "feed/SubscriptionManager.hpp"

#include "data/Types.hpp"
#include "feed/Types.hpp"

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
void
SubscriptionManager::subBookChanges(SubscriberSharedPtr const& subscriber)
{
    bookChangesFeed_.sub(subscriber);
}

void
SubscriptionManager::unsubBookChanges(SubscriberSharedPtr const& subscriber)
{
    bookChangesFeed_.unsub(subscriber);
}

void
SubscriptionManager::pubBookChanges(
    ripple::LedgerHeader const& lgrInfo,
    std::vector<data::TransactionAndMetadata> const& transactions
) const
{
    bookChangesFeed_.pub(lgrInfo, transactions);
}

void
SubscriptionManager::subProposedTransactions(SubscriberSharedPtr const& subscriber)
{
    proposedTransactionFeed_.sub(subscriber);
    // proposed_transactions subscribers not only receive the transaction json when it is proposed, but also the
    // transaction json when it is validated. So the subscriber also subscribes to the transaction feed.
    transactionFeed_.subProposed(subscriber);
}

void
SubscriptionManager::unsubProposedTransactions(SubscriberSharedPtr const& subscriber)
{
    proposedTransactionFeed_.unsub(subscriber);
    transactionFeed_.unsubProposed(subscriber);
}

void
SubscriptionManager::subProposedAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    proposedTransactionFeed_.sub(account, subscriber);
    // Same as proposed_transactions subscribers, proposed_account subscribers also subscribe to the transaction feed to
    // receive validated transaction feed. TransactionFeed class will filter out the sessions that have been sent to.
    transactionFeed_.subProposed(account, subscriber);
}

void
SubscriptionManager::unsubProposedAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    proposedTransactionFeed_.unsub(account, subscriber);
    transactionFeed_.unsubProposed(account, subscriber);
}

void
SubscriptionManager::forwardProposedTransaction(boost::json::object const& receivedTxJson)
{
    proposedTransactionFeed_.pub(receivedTxJson);
}

boost::json::object
SubscriptionManager::subLedger(boost::asio::yield_context yield, SubscriberSharedPtr const& subscriber)
{
    return ledgerFeed_.sub(yield, backend_, subscriber);
}

void
SubscriptionManager::unsubLedger(SubscriberSharedPtr const& subscriber)
{
    ledgerFeed_.unsub(subscriber);
}

void
SubscriptionManager::pubLedger(
    ripple::LedgerHeader const& lgrInfo,
    ripple::Fees const& fees,
    std::string const& ledgerRange,
    std::uint32_t const txnCount
) const
{
    ledgerFeed_.pub(lgrInfo, fees, ledgerRange, txnCount);
}

void
SubscriptionManager::subManifest(SubscriberSharedPtr const& subscriber)
{
    manifestFeed_.sub(subscriber);
}

void
SubscriptionManager::unsubManifest(SubscriberSharedPtr const& subscriber)
{
    manifestFeed_.unsub(subscriber);
}

void
SubscriptionManager::forwardManifest(boost::json::object const& manifestJson) const
{
    manifestFeed_.pub(manifestJson);
}

void
SubscriptionManager::subValidation(SubscriberSharedPtr const& subscriber)
{
    validationsFeed_.sub(subscriber);
}

void
SubscriptionManager::unsubValidation(SubscriberSharedPtr const& subscriber)
{
    validationsFeed_.unsub(subscriber);
}

void
SubscriptionManager::forwardValidation(boost::json::object const& validationJson) const
{
    validationsFeed_.pub(validationJson);
}

void
SubscriptionManager::subTransactions(SubscriberSharedPtr const& subscriber)
{
    transactionFeed_.sub(subscriber);
}

void
SubscriptionManager::unsubTransactions(SubscriberSharedPtr const& subscriber)
{
    transactionFeed_.unsub(subscriber);
}

void
SubscriptionManager::subAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    transactionFeed_.sub(account, subscriber);
}

void
SubscriptionManager::unsubAccount(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    transactionFeed_.unsub(account, subscriber);
}

void
SubscriptionManager::subBook(ripple::Book const& book, SubscriberSharedPtr const& subscriber)
{
    transactionFeed_.sub(book, subscriber);
}

void
SubscriptionManager::unsubBook(ripple::Book const& book, SubscriberSharedPtr const& subscriber)
{
    transactionFeed_.unsub(book, subscriber);
}

void
SubscriptionManager::pubTransaction(data::TransactionAndMetadata const& txMeta, ripple::LedgerHeader const& lgrInfo)
{
    transactionFeed_.pub(txMeta, lgrInfo, backend_);
}

boost::json::object
SubscriptionManager::report() const
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

}  // namespace feed
