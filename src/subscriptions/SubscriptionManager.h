#ifndef SUBSCRIPTION_MANAGER_H
#define SUBSCRIPTION_MANAGER_H

#include <backend/BackendInterface.h>
#include <memory>
#include <subscriptions/Message.h>

class WsBase;

class Subscription
{
    boost::asio::io_context::strand strand_;
    std::unordered_set<std::shared_ptr<WsBase>> subscribers_ = {};
    std::atomic_uint64_t subCount_ = 0;

public:
    Subscription() = delete;
    Subscription(Subscription&) = delete;
    Subscription(Subscription&&) = delete;

    explicit Subscription(boost::asio::io_context& ioc) : strand_(ioc)
    {
    }

    ~Subscription() = default;

    void
    subscribe(std::shared_ptr<WsBase> const& session);

    void
    unsubscribe(std::shared_ptr<WsBase> const& session);

    void
    publish(std::shared_ptr<Message> const& message);

    std::uint64_t
    count() const
    {
        return subCount_.load();
    }

    bool
    empty() const
    {
        return count() == 0;
    }
};

template <class Key>
class SubscriptionMap
{
    using ptr = std::shared_ptr<WsBase>;
    using subscribers = std::set<ptr>;

    boost::asio::io_context::strand strand_;
    std::unordered_map<Key, subscribers> subscribers_ = {};
    std::atomic_uint64_t subCount_ = 0;

public:
    SubscriptionMap() = delete;
    SubscriptionMap(SubscriptionMap&) = delete;
    SubscriptionMap(SubscriptionMap&&) = delete;

    explicit SubscriptionMap(boost::asio::io_context& ioc) : strand_(ioc)
    {
    }

    ~SubscriptionMap() = default;

    void
    subscribe(std::shared_ptr<WsBase> const& session, Key const& key);

    void
    unsubscribe(std::shared_ptr<WsBase> const& session, Key const& key);

    void
    publish(std::shared_ptr<Message>& message, Key const& key);

    std::uint64_t
    count()
    {
        return subCount_.load();
    }
};

class SubscriptionManager
{
    using session_ptr = std::shared_ptr<WsBase>;

    std::vector<std::thread> workers_;
    boost::asio::io_context ioc_;
    std::optional<boost::asio::io_context::work> work_;

    Subscription ledgerSubscribers_;
    Subscription txSubscribers_;
    Subscription txProposedSubscribers_;
    Subscription manifestSubscribers_;
    Subscription validationsSubscribers_;
    Subscription bookChangesSubscribers_;

    SubscriptionMap<ripple::AccountID> accountSubscribers_;
    SubscriptionMap<ripple::AccountID> accountProposedSubscribers_;
    SubscriptionMap<ripple::Book> bookSubscribers_;

    std::shared_ptr<Backend::BackendInterface const> backend_;

public:
    static std::shared_ptr<SubscriptionManager>
    make_SubscriptionManager(
        boost::json::object const& config,
        std::shared_ptr<Backend::BackendInterface const> const& b)
    {
        auto numThreads = 1;

        if (config.contains("subscription_workers") &&
            config.at("subscription_workers").is_int64())
        {
            numThreads = config.at("subscription_workers").as_int64();
        }

        return std::make_shared<SubscriptionManager>(numThreads, b);
    }

    SubscriptionManager(
        std::uint64_t numThreads,
        std::shared_ptr<Backend::BackendInterface const> const& b)
        : ledgerSubscribers_(ioc_)
        , txSubscribers_(ioc_)
        , txProposedSubscribers_(ioc_)
        , manifestSubscribers_(ioc_)
        , validationsSubscribers_(ioc_)
        , bookChangesSubscribers_(ioc_)
        , accountSubscribers_(ioc_)
        , accountProposedSubscribers_(ioc_)
        , bookSubscribers_(ioc_)
        , backend_(b)
    {
        work_.emplace(ioc_);

        // We will eventually want to clamp this to be the number of strands,
        // since adding more threads than we have strands won't see any
        // performance benefits
        BOOST_LOG_TRIVIAL(info) << "Starting subscription manager with "
                                << numThreads << " workers";

        workers_.reserve(numThreads);
        for (auto i = numThreads; i > 0; --i)
            workers_.emplace_back([this] { ioc_.run(); });
    }

    ~SubscriptionManager()
    {
        work_.reset();

        ioc_.stop();
        for (auto& worker : workers_)
            worker.join();
    }

    boost::json::object
    subLedger(boost::asio::yield_context& yield, session_ptr session);

    void
    pubLedger(
        ripple::LedgerInfo const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        std::uint32_t txnCount);

    void
    pubBookChanges(
        ripple::LedgerInfo const& lgrInfo,
        std::vector<Backend::TransactionAndMetadata> const& transactions);

    void
    unsubLedger(session_ptr session);

    void
    subTransactions(session_ptr session);

    void
    unsubTransactions(session_ptr session);

    void
    pubTransaction(
        Backend::TransactionAndMetadata const& blobs,
        ripple::LedgerInfo const& lgrInfo);

    void
    subAccount(ripple::AccountID const& account, session_ptr& session);

    void
    unsubAccount(ripple::AccountID const& account, session_ptr& session);

    void
    subBook(ripple::Book const& book, session_ptr session);

    void
    unsubBook(ripple::Book const& book, session_ptr session);

    void
    subBookChanges(std::shared_ptr<WsBase> session);

    void
    unsubBookChanges(std::shared_ptr<WsBase> session);

    void
    subManifest(session_ptr session);

    void
    unsubManifest(session_ptr session);

    void
    subValidation(session_ptr session);

    void
    unsubValidation(session_ptr session);

    void
    forwardProposedTransaction(boost::json::object const& response);

    void
    forwardManifest(boost::json::object const& response);

    void
    forwardValidation(boost::json::object const& response);

    void
    subProposedAccount(ripple::AccountID const& account, session_ptr session);

    void
    unsubProposedAccount(ripple::AccountID const& account, session_ptr session);

    void
    subProposedTransactions(session_ptr session);

    void
    unsubProposedTransactions(session_ptr session);

    void
    cleanup(session_ptr session);

    boost::json::object
    report()
    {
        boost::json::object counts = {};

        counts["ledger"] = ledgerSubscribers_.count();
        counts["transactions"] = txSubscribers_.count();
        counts["transactions_proposed"] = txProposedSubscribers_.count();
        counts["manifests"] = manifestSubscribers_.count();
        counts["validations"] = validationsSubscribers_.count();
        counts["account"] = accountSubscribers_.count();
        counts["accounts_proposed"] = accountProposedSubscribers_.count();
        counts["books"] = bookSubscribers_.count();
        counts["book_changes"] = bookChangesSubscribers_.count();

        return counts;
    }

private:
    void
    sendAll(std::string const& pubMsg, std::unordered_set<session_ptr>& subs);

    /**
     * This is how we chose to cleanup subscriptions that have been closed.
     * Each time we add a subscriber, we add the opposite lambda that
     * unsubscribes that subscriber when cleanup is called with the session that
     * closed.
     */
    using CleanupFunction = std::function<void(session_ptr)>;
    std::mutex cleanupMtx_;
    std::unordered_map<session_ptr, std::vector<CleanupFunction>>
        cleanupFuncs_ = {};
};

#endif  // SUBSCRIPTION_MANAGER_H
