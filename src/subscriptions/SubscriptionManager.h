#ifndef SUBSCRIPTION_MANAGER_H
#define SUBSCRIPTION_MANAGER_H

#include <backend/BackendInterface.h>
#include <memory>

class WsBase;

class Subscription
{
    boost::asio::io_context::strand strand_;
    std::unordered_set<std::shared_ptr<WsBase>> subscribers_ = {};

    void
    sendAll(std::string const& pubMsg);

public:
    static std::unique_ptr<Subscription>
    make_Subscription(boost::asio::io_context& ioc) {
        return std::make_unique<Subscription>(ioc);
    }

    Subscription() = delete;
    Subscription(Subscription&) = delete;
    Subscription(Subscription&&) = delete;

    explicit 
    Subscription(boost::asio::io_context& ioc) : strand_(ioc)
    {    
    }

    ~Subscription() = default;

    void
    subscribe(std::shared_ptr<WsBase> const& session);

    void
    unsubscribe(std::shared_ptr<WsBase> const& session);

    void
    publish(std::string const& message);
};

class AccountSubscription
{
    boost::asio::io_context::strand strand_;
    std::unordered_map<
        ripple::AccountID,
        std::unordered_set<std::shared_ptr<WsBase>>> subscribers_ = {};

    void
    sendAll(std::string const& pubMsg, ripple::AccountID const& account);

public:
    static std::unique_ptr<AccountSubscription>
    make_Subscription(boost::asio::io_context& ioc) {
        return std::make_unique<AccountSubscription>(ioc);
    }

    AccountSubscription() = delete;
    AccountSubscription(AccountSubscription&) = delete;
    AccountSubscription(AccountSubscription&&) = delete;

    explicit 
    AccountSubscription(boost::asio::io_context& ioc) : strand_(ioc) 
    {
    }

    ~AccountSubscription() = default;

    void
    subscribe(
        std::shared_ptr<WsBase> const& session,
        ripple::AccountID const& account);

    void
    unsubscribe(
        std::shared_ptr<WsBase> const& session,
        ripple::AccountID const& account);

    void
    publish(
        std::string const& message,
        ripple::AccountID const& account);
};

class BookSubscription
{

    boost::asio::io_context::strand strand_;
    std::unordered_map<
        ripple::Book,
        std::unordered_set<std::shared_ptr<WsBase>>> subscribers_ = {};

    void
    sendAll(std::string const& pubMsg, ripple::Book const& book);

public:
    static std::unique_ptr<BookSubscription>
    make_Subscription(boost::asio::io_context& ioc) {
        return std::make_unique<BookSubscription>(ioc);
    }

    BookSubscription() = delete;
    BookSubscription(BookSubscription&) = delete;
    BookSubscription(BookSubscription&&) = delete;

    explicit 
    BookSubscription(boost::asio::io_context& ioc) : strand_(ioc) 
    {
    }

    ~BookSubscription() = default;

    void
    subscribe(
        std::shared_ptr<WsBase> const& session,
        ripple::Book const& book);

    void
    unsubscribe(
        std::shared_ptr<WsBase> const& session,
        ripple::Book const& book);

    void
    publish(
        std::string const& message,
        ripple::Book const& book);
};

class SubscriptionManager
{
    using subscription_ptr = std::unique_ptr<Subscription>;

    enum SubscriptionType {
        Ledgers,
        Transactions,
        TransactionsProposed,
        Manifests,
        Validations,

        finalEntry
    };
    
    std::vector<std::thread> workers_;
    boost::asio::io_context ioc_;
    std::optional<boost::asio::io_context::work> work_;
    std::thread thread_;

    std::vector<subscription_ptr> streamSubscribers_;
    
    std::unique_ptr<AccountSubscription> accountSubscribers_;
    std::unique_ptr<AccountSubscription> accountProposedSubscribers_;
    std::unique_ptr<BookSubscription> bookSubscribers_;
    std::shared_ptr<Backend::BackendInterface> backend_;

public:
    static std::shared_ptr<SubscriptionManager>
    make_SubscriptionManager(
        boost::json::object const& config,
        std::shared_ptr<Backend::BackendInterface> const& b)
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
        std::shared_ptr<Backend::BackendInterface> const& b)
        : accountSubscribers_(AccountSubscription::make_Subscription(ioc_))
        , accountProposedSubscribers_(AccountSubscription::make_Subscription(ioc_))
        , bookSubscribers_(BookSubscription::make_Subscription(ioc_))
        , backend_(b)
    {
        for (auto i = 0; i < finalEntry; ++i)
        {
            streamSubscribers_.push_back(Subscription::make_Subscription(ioc_));
        }

        work_.emplace(ioc_);

        BOOST_LOG_TRIVIAL(info)
             << "Starting subscription manager with " << numThreads << " workers";

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
