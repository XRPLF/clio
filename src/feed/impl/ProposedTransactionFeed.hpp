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

#include "feed/Types.hpp"
#include "feed/impl/TrackableSignal.hpp"
#include "feed/impl/TrackableSignalMap.hpp"
#include "feed/impl/Util.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Gauge.hpp"

#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <fmt/core.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

namespace feed::impl {

/**
 * @brief Feed that publishes the Proposed Transactions.
 *
 * @tparam ExecutionContext The type of the execution context.
 * @note Be aware that the Clio only forwards this stream, not respect api_version.
 */
template <class ExecutionContext>
class ProposedTransactionFeed {
    util::Logger logger_{"Subscriptions"};

    std::unordered_set<SubscriberPtr>
        notified_;  // Used by slots to prevent double notifications if tx contains multiple subscribed accounts
    ExecutionContext::Strand strand_;
    std::reference_wrapper<util::prometheus::GaugeInt> subAllCount_;
    std::reference_wrapper<util::prometheus::GaugeInt> subAccountCount_;

    TrackableSignalMap<ripple::AccountID, Subscriber, std::shared_ptr<std::string>> accountSignal_;
    TrackableSignal<Subscriber, std::shared_ptr<std::string>> signal_;

public:
    /**
     * @brief Construct a Proposed Transaction Feed object.
     *
     * @param executorContext The actual publish will be called in the strand of this.
     */
    ProposedTransactionFeed(ExecutionContext& executorContext)
        : strand_(executorContext.makeStrand())
        , subAllCount_(getSubscriptionsGaugeInt("tx_proposed"))
        , subAccountCount_(getSubscriptionsGaugeInt("account_proposed"))

    {
    }

    /**
     * @brief Subscribe to the proposed transaction feed.
     *
     * @param subscriber
     */
    void
    sub(SubscriberSharedPtr const& subscriber)
    {
        auto const weakPtr = std::weak_ptr(subscriber);
        auto const added = signal_.connectTrackableSlot(subscriber, [weakPtr](std::shared_ptr<std::string> const& msg) {
            if (auto connectionPtr = weakPtr.lock()) {
                connectionPtr->send(msg);
            }
        });

        if (added) {
            LOG(logger_.info()) << subscriber->tag() << "Subscribed tx_proposed";
            ++subAllCount_.get();
            subscriber->onDisconnect.connect([this](SubscriberPtr connection) { unsubInternal(connection); });
        }
    }

    /**
     * @brief Subscribe to the proposed transaction feed, only receive the feed when particular account is affected.
     *
     * @param subscriber
     * @param account The account to watch.
     */
    void
    sub(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
    {
        auto const weakPtr = std::weak_ptr(subscriber);
        auto const added = accountSignal_.connectTrackableSlot(
            subscriber,
            account,
            [this, weakPtr](std::shared_ptr<std::string> const& msg) {
                if (auto connectionPtr = weakPtr.lock()) {
                    // Check if this connection already sent
                    if (notified_.contains(connectionPtr.get()))
                        return;

                    notified_.insert(connectionPtr.get());
                    connectionPtr->send(msg);
                }
            }
        );
        if (added) {
            LOG(logger_.info()) << subscriber->tag() << "Subscribed accounts_proposed " << account;
            ++subAccountCount_.get();
            subscriber->onDisconnect.connect([this, account](SubscriberPtr connection) {
                unsubInternal(account, connection);
            });
        }
    }

    /**
     * @brief Unsubscribe to the proposed transaction feed.
     *
     * @param subscriber
     */
    void
    unsub(SubscriberSharedPtr const& subscriber)
    {
        unsubInternal(subscriber.get());
    }

    /**
     * @brief Unsubscribe to the proposed transaction feed for particular account.
     *
     * @param subscriber
     * @param account The account to unsubscribe.
     */
    void
    unsub(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
    {
        unsubInternal(account, subscriber.get());
    }

    /**
     * @brief Publishes the proposed transaction feed.
     *
     * @param receivedTxJson The proposed transaction json.
     */
    void
    pub(boost::json::object const& receivedTxJson)
    {
        auto pubMsg = std::make_shared<std::string>(boost::json::serialize(receivedTxJson));

        auto const transaction = receivedTxJson.at("transaction").as_object();
        auto const accounts = rpc::getAccountsFromTransaction(transaction);
        auto affectedAccounts = std::unordered_set<ripple::AccountID>(accounts.cbegin(), accounts.cend());

        [[maybe_unused]] auto task =
            strand_.execute([this, pubMsg = std::move(pubMsg), affectedAccounts = std::move(affectedAccounts)]() {
                notified_.clear();
                signal_.emit(pubMsg);
                // Prevent the same connection from receiving the same message twice if it is subscribed to multiple
                // accounts However, if the same connection subscribe both stream and account, it will still receive the
                // message twice. notified_ can be cleared before signal_ emit to improve this, but let's keep it as is
                // for now, since rippled acts like this.
                notified_.clear();
                for (auto const& account : affectedAccounts)
                    accountSignal_.emit(account, pubMsg);
            });
    }

    /**
     * @brief Get the number of subscribers of the proposed transaction feed.
     */
    std::uint64_t
    transactionSubcount() const
    {
        return subAllCount_.get().value();
    }
    /**
     * @brief Get the number of accounts subscribers.
     */
    std::uint64_t
    accountSubCount() const
    {
        return subAccountCount_.get().value();
    }

private:
    void
    unsubInternal(SubscriberPtr subscriber)
    {
        if (signal_.disconnect(subscriber)) {
            LOG(logger_.info()) << subscriber->tag() << "Unsubscribed tx_proposed";
            --subAllCount_.get();
        }
    }

    void
    unsubInternal(ripple::AccountID const& account, SubscriberPtr subscriber)
    {
        if (accountSignal_.disconnect(subscriber, account)) {
            LOG(logger_.info()) << subscriber->tag() << "Unsubscribed accounts_proposed " << account;
            --subAccountCount_.get();
        }
    }
};
}  // namespace feed::impl
