#ifndef SUBSCRIPTION_MANAGER_H
#define SUBSCRIPTION_MANAGER_H

#include <backend/BackendInterface.h>
#include <memory>

class WsBase;

class SubscriptionManager
{
    using subscriptions = std::unordered_set<std::shared_ptr<WsBase>>;

    enum SubscriptionType {
        Ledgers,
        Transactions,
        TransactionsProposed,
        Manifests,
        Validations,

        finalEntry
    };
    std::mutex m_;
    std::array<subscriptions, finalEntry> streamSubscribers_;
    std::unordered_map<ripple::AccountID, subscriptions> accountSubscribers_;
    std::unordered_map<ripple::AccountID, subscriptions>
        accountProposedSubscribers_;
    std::unordered_map<ripple::Book, subscriptions> bookSubscribers_;
    std::shared_ptr<Backend::BackendInterface> backend_;

public:
    static std::shared_ptr<SubscriptionManager>
    make_SubscriptionManager(
        std::shared_ptr<Backend::BackendInterface> const& b)
    {
        return std::make_shared<SubscriptionManager>(b);
    }

    SubscriptionManager(std::shared_ptr<Backend::BackendInterface> const& b)
        : backend_(b)
    {
    }

    boost::json::object
    subLedger(std::shared_ptr<WsBase>& session);

    void
    pubLedger(
        ripple::LedgerInfo const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        std::uint32_t txnCount);

    void
    unsubLedger(std::shared_ptr<WsBase>& session);

    void
    subTransactions(std::shared_ptr<WsBase>& session);

    void
    unsubTransactions(std::shared_ptr<WsBase>& session);

    void
    pubTransaction(
        Backend::TransactionAndMetadata const& blobs,
        ripple::LedgerInfo const& lgrInfo);

    void
    subAccount(
        ripple::AccountID const& account,
        std::shared_ptr<WsBase>& session);

    void
    unsubAccount(
        ripple::AccountID const& account,
        std::shared_ptr<WsBase>& session);

    void
    subBook(ripple::Book const& book, std::shared_ptr<WsBase>& session);

    void
    unsubBook(ripple::Book const& book, std::shared_ptr<WsBase>& session);

    void
    subManifest(std::shared_ptr<WsBase>& session);

    void
    unsubManifest(std::shared_ptr<WsBase>& session);

    void
    subValidation(std::shared_ptr<WsBase>& session);

    void
    unsubValidation(std::shared_ptr<WsBase>& session);

    void
    forwardProposedTransaction(boost::json::object const& response);

    void
    forwardManifest(boost::json::object const& response);

    void
    forwardValidation(boost::json::object const& response);

    void
    subProposedAccount(
        ripple::AccountID const& account,
        std::shared_ptr<WsBase>& session);

    void
    unsubProposedAccount(
        ripple::AccountID const& account,
        std::shared_ptr<WsBase>& session);

    void
    subProposedTransactions(std::shared_ptr<WsBase>& session);

    void
    unsubProposedTransactions(std::shared_ptr<WsBase>& session);

private:
    void
    sendAll(
        std::string const& pubMsg,
        std::unordered_set<std::shared_ptr<WsBase>>& subs);
};

#endif  // SUBSCRIPTION_MANAGER_H
