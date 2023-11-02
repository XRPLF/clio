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

#include <data/BackendInterface.h>
#include <util/config/Config.h>
#include <util/log/Logger.h>
#include <util/prometheus/Prometheus.h>
#include <web/interface/ConnectionBase.h>

#include <ripple/protocol/LedgerHeader.h>

#include <memory>

/**
 * @brief This namespace deals with subscriptions.
 */
namespace feed {

using SessionPtrType = std::shared_ptr<web::ConnectionBase>;

/**
 * @brief Sends a message to subscribers.
 *
 * @param message The message to send
 * @param subscribers The subscription stream to send the message to
 * @param counter The subscription counter to decrement if session is detected as dead
 */
template <class T>
inline void
sendToSubscribers(std::shared_ptr<std::string> const& message, T& subscribers, util::prometheus::GaugeInt& counter)
{
    for (auto it = subscribers.begin(); it != subscribers.end();) {
        auto& session = *it;
        if (session->dead()) {
            it = subscribers.erase(it);
            --counter;
        } else {
            session->send(message);
            ++it;
        }
    }
}

/**
 * @brief Adds a session to the subscription stream.
 *
 * @param session The session to add
 * @param subscribers The stream to subscribe to
 * @param counter The counter representing the current total subscribers
 */
template <class T>
inline void
addSession(SessionPtrType session, T& subscribers, util::prometheus::GaugeInt& counter)
{
    if (!subscribers.contains(session)) {
        subscribers.insert(session);
        ++counter;
    }
}

/**
 * @brief Removes a session from the subscription stream.
 *
 * @param session The session to remove
 * @param subscribers The stream to unsubscribe from
 * @param counter The counter representing the current total subscribers
 */
template <class T>
inline void
removeSession(SessionPtrType session, T& subscribers, util::prometheus::GaugeInt& counter)
{
    if (subscribers.contains(session)) {
        subscribers.erase(session);
        --counter;
    }
}

/**
 * @brief Represents a subscription stream.
 */
class Subscription {
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    std::unordered_set<SessionPtrType> subscribers_ = {};
    util::prometheus::GaugeInt& subCount_;

public:
    Subscription() = delete;
    Subscription(Subscription&) = delete;
    Subscription(Subscription&&) = delete;

    /**
     * @brief Create a new subscription stream.
     *
     * @param ioc The io_context to run on
     */
    explicit Subscription(boost::asio::io_context& ioc, std::string const& name)
        : strand_(boost::asio::make_strand(ioc))
        , subCount_(PrometheusService::gaugeInt(
              "subscriptions_current_number",
              util::prometheus::Labels({util::prometheus::Label{"stream", name}}),
              fmt::format("Current subscribers number on the {} stream", name)
          ))
    {
    }

    ~Subscription() = default;

    /**
     * @brief Adds the given session to the subscribers set.
     *
     * @param session The session to add
     */
    void
    subscribe(SessionPtrType const& session);

    /**
     * @brief Removes the given session from the subscribers set.
     *
     * @param session The session to remove
     */
    void
    unsubscribe(SessionPtrType const& session);

    /**
     * @brief Check if a session has been in subscribers list.
     *
     * @param session The session to check
     * @return true if the session is in the subscribers list; false otherwise
     */
    bool
    hasSession(SessionPtrType const& session);

    /**
     * @brief Sends the given message to all subscribers.
     *
     * @param message The message to send
     */
    void
    publish(std::shared_ptr<std::string> const& message);

    /**
     * @return Total subscriber count on this stream.
     */
    std::uint64_t
    count() const
    {
        return subCount_.value();
    }

    /**
     * @return true if the stream currently has no subscribers; false otherwise
     */
    bool
    empty() const
    {
        return count() == 0;
    }
};

/**
 * @brief Represents a collection of subscriptions where each stream is mapped to a key.
 */
template <class Key>
class SubscriptionMap {
    using SubscribersType = std::set<SessionPtrType>;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    std::unordered_map<Key, SubscribersType> subscribers_ = {};
    util::prometheus::GaugeInt& subCount_;

public:
    SubscriptionMap() = delete;
    SubscriptionMap(SubscriptionMap&) = delete;
    SubscriptionMap(SubscriptionMap&&) = delete;

    /**
     * @brief Create a new subscription map.
     *
     * @param ioc The io_context to run on
     */
    explicit SubscriptionMap(boost::asio::io_context& ioc, std::string const& name)
        : strand_(boost::asio::make_strand(ioc))
        , subCount_(PrometheusService::gaugeInt(
              "subscriptions_current_number",
              util::prometheus::Labels({util::prometheus::Label{"collection", name}}),
              fmt::format("Current subscribers number on the {} collection", name)
          ))
    {
    }

    ~SubscriptionMap() = default;

    /**
     * @brief Subscribe to a specific stream by its key.
     *
     * @param session The session to add
     * @param key The key for the subscription to subscribe to
     */
    void
    subscribe(SessionPtrType const& session, Key const& key)
    {
        boost::asio::post(strand_, [this, session, key]() { addSession(session, subscribers_[key], subCount_); });
    }

    /**
     * @brief Unsubscribe from a specific stream by its key.
     *
     * @param session The session to remove
     * @param key The key for the subscription to unsubscribe from
     */
    void
    unsubscribe(SessionPtrType const& session, Key const& key)
    {
        boost::asio::post(strand_, [this, key, session]() {
            if (!subscribers_.contains(key))
                return;

            if (!subscribers_[key].contains(session))
                return;

            --subCount_;
            subscribers_[key].erase(session);

            if (subscribers_[key].size() == 0) {
                subscribers_.erase(key);
            }
        });
    }

    /**
     * @brief Check if a session has been in subscribers list.
     *
     * @param session The session to check
     * @param key The key for the subscription to check
     * @return true if the session is in the subscribers list; false otherwise
     */
    bool
    hasSession(SessionPtrType const& session, Key const& key)
    {
        if (!subscribers_.contains(key))
            return false;

        return subscribers_[key].contains(session);
    }

    /**
     * @brief Sends the given message to all subscribers.
     *
     * @param message The message to send
     * @param key The key for the subscription to send the message to
     */
    void
    publish(std::shared_ptr<std::string> const& message, Key const& key)
    {
        boost::asio::post(strand_, [this, key, message]() {
            if (!subscribers_.contains(key))
                return;

            sendToSubscribers(message, subscribers_[key], subCount_);
        });
    }

    /**
     * @return Total subscriber count on all streams in the collection.
     */
    std::uint64_t
    count() const
    {
        return subCount_.value();
    }
};

/**
 * @brief Manages subscriptions.
 */
class SubscriptionManager {
    util::Logger log_{"Subscriptions"};

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

    std::shared_ptr<data::BackendInterface const> backend_;

public:
    /**
     * @brief A factory function that creates a new subscription manager configured from the config provided.
     *
     * @param config The configuration to use
     * @param backend The backend to use
     */
    static std::shared_ptr<SubscriptionManager>
    make_SubscriptionManager(util::Config const& config, std::shared_ptr<data::BackendInterface const> const& backend)
    {
        auto numThreads = config.valueOr<uint64_t>("subscription_workers", 1);
        return std::make_shared<SubscriptionManager>(numThreads, backend);
    }

    /**
     * @brief Creates a new instance of the subscription manager.
     *
     * @param numThreads The number of worker threads to manage subscriptions
     * @param backend The backend to use
     */
    SubscriptionManager(std::uint64_t numThreads, std::shared_ptr<data::BackendInterface const> const& backend)
        : ledgerSubscribers_(ioc_, "ledger")
        , txSubscribers_(ioc_, "tx")
        , txProposedSubscribers_(ioc_, "tx_proposed")
        , manifestSubscribers_(ioc_, "manifest")
        , validationsSubscribers_(ioc_, "validations")
        , bookChangesSubscribers_(ioc_, "book_changes")
        , accountSubscribers_(ioc_, "account")
        , accountProposedSubscribers_(ioc_, "account_proposed")
        , bookSubscribers_(ioc_, "book")
        , backend_(backend)
    {
        work_.emplace(ioc_);

        // We will eventually want to clamp this to be the number of strands,
        // since adding more threads than we have strands won't see any
        // performance benefits
        LOG(log_.info()) << "Starting subscription manager with " << numThreads << " workers";

        workers_.reserve(numThreads);
        for (auto i = numThreads; i > 0; --i)
            workers_.emplace_back([this] { ioc_.run(); });
    }

    /** @brief Stops the worker threads of the subscription manager. */
    ~SubscriptionManager()
    {
        work_.reset();

        ioc_.stop();
        for (auto& worker : workers_)
            worker.join();
    }

    /**
     * @brief Subscribe to the ledger stream.
     *
     * @param yield The coroutine context
     * @param session The session to subscribe to the stream
     * @return JSON object representing the first message to be sent to the new subscriber
     */
    boost::json::object
    subLedger(boost::asio::yield_context yield, SessionPtrType session);

    /**
     * @brief Publish to the ledger stream.
     *
     * @param lgrInfo The ledger header to serialize
     * @param fees The fees to serialize
     * @param ledgerRange The ledger range this message applies to
     * @param txnCount The total number of transactions to serialize
     */
    void
    pubLedger(
        ripple::LedgerHeader const& lgrInfo,
        ripple::Fees const& fees,
        std::string const& ledgerRange,
        std::uint32_t txnCount
    );

    /**
     * @brief Publish to the book changes stream.
     *
     * @param lgrInfo The ledger header to serialize
     * @param transactions The transactions to serialize
     */
    void
    pubBookChanges(ripple::LedgerHeader const& lgrInfo, std::vector<data::TransactionAndMetadata> const& transactions);

    /**
     * @brief Unsubscribe from the ledger stream.
     *
     * @param session The session to unsubscribe from the stream
     */
    void
    unsubLedger(SessionPtrType session);

    /**
     * @brief Subscribe to the transactions stream.
     *
     * @param session The session to subscribe to the stream
     */
    void
    subTransactions(SessionPtrType session);

    /**
     * @brief Unsubscribe from the transactions stream.
     *
     * @param session The session to unsubscribe from the stream
     */
    void
    unsubTransactions(SessionPtrType session);

    /**
     * @brief Publish to the book changes stream.
     *
     * @param blobs The transactions to serialize
     * @param lgrInfo The ledger header to serialize
     */
    void
    pubTransaction(data::TransactionAndMetadata const& blobs, ripple::LedgerHeader const& lgrInfo);

    /**
     * @brief Subscribe to the account changes stream.
     *
     * @param account The account to monitor changes for
     * @param session The session to subscribe to the stream
     */
    void
    subAccount(ripple::AccountID const& account, SessionPtrType const& session);

    /**
     * @brief Unsubscribe from the account changes stream.
     *
     * @param account The account the stream is for
     * @param session The session to unsubscribe from the stream
     */
    void
    unsubAccount(ripple::AccountID const& account, SessionPtrType const& session);

    /**
     * @brief Subscribe to a specific book changes stream.
     *
     * @param book The book to monitor changes for
     * @param session The session to subscribe to the stream
     */
    void
    subBook(ripple::Book const& book, SessionPtrType session);

    /**
     * @brief Unsubscribe from the specific book changes stream.
     *
     * @param book The book to stop monitoring changes for
     * @param session The session to unsubscribe from the stream
     */
    void
    unsubBook(ripple::Book const& book, SessionPtrType session);

    /**
     * @brief Subscribe to the book changes stream.
     *
     * @param session The session to subscribe to the stream
     */
    void
    subBookChanges(SessionPtrType session);

    /**
     * @brief Unsubscribe from the book changes stream.
     *
     * @param session The session to unsubscribe from the stream
     */
    void
    unsubBookChanges(SessionPtrType session);

    /**
     * @brief Subscribe to the manifest stream.
     *
     * @param session The session to subscribe to the stream
     */
    void
    subManifest(SessionPtrType session);

    /**
     * @brief Unsubscribe from the manifest stream.
     *
     * @param session The session to unsubscribe from the stream
     */
    void
    unsubManifest(SessionPtrType session);

    /**
     * @brief Subscribe to the validation stream.
     *
     * @param session The session to subscribe to the stream
     */
    void
    subValidation(SessionPtrType session);

    /**
     * @brief Unsubscribe from the validation stream.
     *
     * @param session The session to unsubscribe from the stream
     */
    void
    unsubValidation(SessionPtrType session);

    /**
     * @brief Publish proposed transactions and proposed accounts from a JSON response.
     *
     * @param response The JSON response to use
     */
    void
    forwardProposedTransaction(boost::json::object const& response);

    /**
     * @brief Publish manifest updates from a JSON response.
     *
     * @param response The JSON response to use
     */
    void
    forwardManifest(boost::json::object const& response);

    /**
     * @brief Publish validation updates from a JSON response.
     *
     * @param response The JSON response to use
     */
    void
    forwardValidation(boost::json::object const& response);

    /**
     * @brief Subscribe to the proposed account stream.
     *
     * @param account The account to monitor
     * @param session The session to subscribe to the stream
     */
    void
    subProposedAccount(ripple::AccountID const& account, SessionPtrType session);

    /**
     * @brief Unsubscribe from the proposed account stream.
     *
     * @param account The account the stream is for
     * @param session The session to unsubscribe from the stream
     */
    void
    unsubProposedAccount(ripple::AccountID const& account, SessionPtrType session);

    /**
     * @brief Subscribe to the processed transactions stream.
     *
     * @param session The session to subscribe to the stream
     */
    void
    subProposedTransactions(SessionPtrType session);

    /**
     * @brief Unsubscribe from the proposed transactions stream.
     *
     * @param session The session to unsubscribe from the stream
     */
    void
    unsubProposedTransactions(SessionPtrType session);

    /** @brief Clenup the session on removal. */
    void
    cleanup(SessionPtrType session);

    /**
     * @brief Generate a JSON report on the current state of the subscriptions.
     *
     * @return The report as a JSON object
     */
    boost::json::object
    report() const
    {
        return {
            {"ledger", ledgerSubscribers_.count()},
            {"transactions", txSubscribers_.count()},
            {"transactions_proposed", txProposedSubscribers_.count()},
            {"manifests", manifestSubscribers_.count()},
            {"validations", validationsSubscribers_.count()},
            {"account", accountSubscribers_.count()},
            {"accounts_proposed", accountProposedSubscribers_.count()},
            {"books", bookSubscribers_.count()},
            {"book_changes", bookChangesSubscribers_.count()},
        };
    }

private:
    using CleanupFunction = std::function<void(SessionPtrType const)>;

    void
    subscribeHelper(SessionPtrType const& session, Subscription& subs, CleanupFunction&& func);

    template <typename Key>
    void
    subscribeHelper(SessionPtrType const& session, Key const& k, SubscriptionMap<Key>& subs, CleanupFunction&& func);

    // This is how we chose to cleanup subscriptions that have been closed.
    // Each time we add a subscriber, we add the opposite lambda that unsubscribes that subscriber when cleanup is
    // called with the session that closed.
    std::mutex cleanupMtx_;
    std::unordered_map<SessionPtrType, std::vector<CleanupFunction>> cleanupFuncs_ = {};
};

}  // namespace feed
