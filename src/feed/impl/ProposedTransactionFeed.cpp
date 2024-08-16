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

#include "feed/impl/ProposedTransactionFeed.hpp"

#include "feed/Types.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/post.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <xrpl/protocol/AccountID.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

namespace feed::impl {

void
ProposedTransactionFeed::sub(SubscriberSharedPtr const& subscriber)
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

void
ProposedTransactionFeed::sub(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
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

void
ProposedTransactionFeed::unsub(SubscriberSharedPtr const& subscriber)
{
    unsubInternal(subscriber.get());
}

void
ProposedTransactionFeed::unsub(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber)
{
    unsubInternal(account, subscriber.get());
}

void
ProposedTransactionFeed::pub(boost::json::object const& receivedTxJson)
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
            // message twice. notified_ can be cleared before signal_ emit to improve this, but let's keep it as is for
            // now, since rippled acts like this.
            notified_.clear();
            for (auto const& account : affectedAccounts)
                accountSignal_.emit(account, pubMsg);
        });
}

std::uint64_t
ProposedTransactionFeed::transactionSubcount() const
{
    return subAllCount_.get().value();
}

std::uint64_t
ProposedTransactionFeed::accountSubCount() const
{
    return subAccountCount_.get().value();
}

void
ProposedTransactionFeed::unsubInternal(SubscriberPtr subscriber)
{
    if (signal_.disconnect(subscriber)) {
        LOG(logger_.info()) << subscriber->tag() << "Unsubscribed tx_proposed";
        --subAllCount_.get();
    }
}

void
ProposedTransactionFeed::unsubInternal(ripple::AccountID const& account, SubscriberPtr subscriber)
{
    if (accountSignal_.disconnect(subscriber, account)) {
        LOG(logger_.info()) << subscriber->tag() << "Unsubscribed accounts_proposed " << account;
        --subAccountCount_.get();
    }
}

}  // namespace feed::impl
