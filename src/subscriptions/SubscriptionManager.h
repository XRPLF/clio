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

#include <backend/BackendInterface.h>
#include <config/Config.h>
#include <log/Logger.h>
#include <webserver2/interface/ConnectionBase.h>

#include <memory>

using SessionPtrType = std::shared_ptr<Server::ConnectionBase>;

class Subscription
{
    boost::asio::io_context::strand strand_;
    std::unordered_set<SessionPtrType> subscribers_ = {};
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
    subscribe(SessionPtrType const& session);

    void
    unsubscribe(SessionPtrType const& session);

    void
    publish(std::shared_ptr<std::string> const& message);

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
    using subscribers = std::set<SessionPtrType>;

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
    subscribe(SessionPtrType const& session, Key const& key);

    void
    unsubscribe(SessionPtrType const& session, Key const& key);

    void
    publish(std::shared_ptr<std::string> const& message, Key const& key);

    std::uint64_t
    count() const
    {
        return subCount_.load();
    }
};

template <class T>
inline void
sendToSubscribers(std::shared_ptr<std::string> const& message, T& subscribers, std::atomic_uint64_t& counter)
{
    for (auto it = subscribers.begin(); it != subscribers.end();)
    {
        auto& session = *it;
        if (session->dead())
        {
            it = subscribers.erase(it);
            --counter;
        }
        else
        {
            session->send(message);
            ++it;
        }
    }
}

template <class T>
inline void
addSession(SessionPtrType session, T& subscribers, std::atomic_uint64_t& counter)
{
    if (!subscribers.contains(session))
    {
        subscribers.insert(session);
        ++counter;
    }
}

template <class T>
inline void
removeSession(SessionPtrType session, T& subscribers, std::atomic_uint64_t& counter)
{
    if (subscribers.contains(session))
    {
        subscribers.erase(session);
        --counter;
    }
}

template <class Key>
void
SubscriptionMap<Key>::subscribe(SessionPtrType const& session, Key const& account)
{
    boost::asio::post(strand_, [this, session, account]() { addSession(session, subscribers_[account], subCount_); });
}

template <class Key>
void
SubscriptionMap<Key>::unsubscribe(SessionPtrType const& session, Key const& account)
{
    boost::asio::post(strand_, [this, account, session]() {
        if (!subscribers_.contains(account))
            return;

        if (!subscribers_[account].contains(session))
            return;

        --subCount_;

        subscribers_[account].erase(session);

        if (subscribers_[account].size() == 0)
        {
            subscribers_.erase(account);
        }
    });
}

template <class Key>
void
SubscriptionMap<Key>::publish(std::shared_ptr<std::string> const& message, Key const& account)
{
    boost::asio::post(strand_, [this, account, message]() {
        if (!subscribers_.contains(account))
            return;

        sendToSubscribers(message, subscribers_[account], subCount_);
    });
}

class SubscriptionManager
{
    clio::Logger log_{"Subscriptions"};

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
    make_SubscriptionManager(clio::Config const& config, std::shared_ptr<Backend::BackendInterface const> const& b)
    {
        auto numThreads = config.valueOr<uint64_t>("subscription_workers", 1);
        return std::make_shared<SubscriptionManager>(numThreads, b);
    }

    SubscriptionManager(std::uint64_t numThreads, std::shared_ptr<Backend::BackendInterface const> const& b)
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
        log_.info() << "Starting subscription manager with " << numThreads << " workers";

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
    subLedger(boost::asio::yield_context& yield, SessionPtrType session);

    void
    pubLedger(
        ripple::LedgerInfo const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        std::uint32_t txnCount);

    void
    pubBookChanges(ripple::LedgerInfo const& lgrInfo, std::vector<Backend::TransactionAndMetadata> const& transactions);

    void
    unsubLedger(SessionPtrType session);

    void
    subTransactions(SessionPtrType session);

    void
    unsubTransactions(SessionPtrType session);

    void
    pubTransaction(Backend::TransactionAndMetadata const& blobs, ripple::LedgerInfo const& lgrInfo);

    void
    subAccount(ripple::AccountID const& account, SessionPtrType const& session);

    void
    unsubAccount(ripple::AccountID const& account, SessionPtrType const& session);

    void
    subBook(ripple::Book const& book, SessionPtrType session);

    void
    unsubBook(ripple::Book const& book, SessionPtrType session);

    void
    subBookChanges(SessionPtrType session);

    void
    unsubBookChanges(SessionPtrType session);

    void
    subManifest(SessionPtrType session);

    void
    unsubManifest(SessionPtrType session);

    void
    subValidation(SessionPtrType session);

    void
    unsubValidation(SessionPtrType session);

    void
    forwardProposedTransaction(boost::json::object const& response);

    void
    forwardManifest(boost::json::object const& response);

    void
    forwardValidation(boost::json::object const& response);

    void
    subProposedAccount(ripple::AccountID const& account, SessionPtrType session);

    void
    unsubProposedAccount(ripple::AccountID const& account, SessionPtrType session);

    void
    subProposedTransactions(SessionPtrType session);

    void
    unsubProposedTransactions(SessionPtrType session);

    void
    cleanup(SessionPtrType session);

    boost::json::object
    report() const
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
    sendAll(std::string const& pubMsg, std::unordered_set<SessionPtrType>& subs);

    using CleanupFunction = std::function<void(SessionPtrType const)>;

    void
    subscribeHelper(SessionPtrType const& session, Subscription& subs, CleanupFunction&& func);

    template <typename Key>
    void
    subscribeHelper(SessionPtrType const& session, Key const& k, SubscriptionMap<Key>& subs, CleanupFunction&& func);

    /**
     * This is how we chose to cleanup subscriptions that have been closed.
     * Each time we add a subscriber, we add the opposite lambda that
     * unsubscribes that subscriber when cleanup is called with the session that
     * closed.
     */
    std::mutex cleanupMtx_;
    std::unordered_map<SessionPtrType, std::vector<CleanupFunction>> cleanupFuncs_ = {};
};
