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
    xrpl::LedgerHeader const& lgrInfo,
    std::vector<data::TransactionAndMetadata> const& transactions
)
{
    bookChangesFeed_.pub(lgrInfo, transactions);
}

void
SubscriptionManager::subProposedTransactions(SubscriberSharedPtr const& subscriber)
{
    proposedTransactionFeed_.sub(subscriber);
    // proposed_transactions subscribers not only receive the transaction json when it is proposed,
    // but also the transaction json when it is validated. So the subscriber also subscribes to the
    // transaction feed.
    transactionFeed_.subProposed(subscriber);
}

void
SubscriptionManager::unsubProposedTransactions(SubscriberSharedPtr const& subscriber)
{
    proposedTransactionFeed_.unsub(subscriber);
    transactionFeed_.unsubProposed(subscriber);
}

void
SubscriptionManager::subProposedAccount(
    xrpl::AccountID const& account,
    SubscriberSharedPtr const& subscriber
)
{
    proposedTransactionFeed_.sub(account, subscriber);
    // Same as proposed_transactions subscribers, proposed_account subscribers also subscribe to the
    // transaction feed to receive validated transaction feed. TransactionFeed class will filter out
    // the sessions that have been sent to.
    transactionFeed_.subProposed(account, subscriber);
}

void
SubscriptionManager::unsubProposedAccount(
    xrpl::AccountID const& account,
    SubscriberSharedPtr const& subscriber
)
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
SubscriptionManager::subLedger(
    boost::asio::yield_context yield,
    SubscriberSharedPtr const& subscriber
)
{
    return ledgerFeed_.sub(yield, backend_, subscriber, networkID_);
}

void
SubscriptionManager::unsubLedger(SubscriberSharedPtr const& subscriber)
{
    ledgerFeed_.unsub(subscriber);
}

void
SubscriptionManager::pubLedger(
    xrpl::LedgerHeader const& lgrInfo,
    xrpl::Fees const& fees,
    std::string const& ledgerRange,
    std::uint32_t const txnCount
)
{
    ledgerFeed_.pub(lgrInfo, fees, ledgerRange, txnCount, networkID_);
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
SubscriptionManager::forwardManifest(boost::json::object const& manifestJson)
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
SubscriptionManager::forwardValidation(boost::json::object const& validationJson)
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
SubscriptionManager::subAccount(
    xrpl::AccountID const& account,
    SubscriberSharedPtr const& subscriber
)
{
    transactionFeed_.sub(account, subscriber);
}

void
SubscriptionManager::unsubAccount(
    xrpl::AccountID const& account,
    SubscriberSharedPtr const& subscriber
)
{
    transactionFeed_.unsub(account, subscriber);
}

void
SubscriptionManager::subBook(xrpl::Book const& book, SubscriberSharedPtr const& subscriber)
{
    transactionFeed_.sub(book, subscriber);
}

void
SubscriptionManager::unsubBook(xrpl::Book const& book, SubscriberSharedPtr const& subscriber)
{
    transactionFeed_.unsub(book, subscriber);
}

void
SubscriptionManager::pubTransaction(
    data::TransactionAndMetadata const& txMeta,
    xrpl::LedgerHeader const& lgrInfo
)
{
    transactionFeed_.pub(txMeta, lgrInfo, backend_, amendmentCenter_, networkID_);
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

void
SubscriptionManager::setNetworkID(uint32_t const networkID)
{
    networkID_ = networkID;
}

uint32_t
SubscriptionManager::getNetworkID() const
{
    return networkID_;
}

}  // namespace feed
