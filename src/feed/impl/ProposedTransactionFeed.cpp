#include "feed/impl/ProposedTransactionFeed.hpp"

#include "feed/Types.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

namespace feed::impl {

void
ProposedTransactionFeed::ProposedTransactionSlot::operator()(
    AllVersionsMsgsPtrType const& allVersionMsgs
) const
{
    if (auto connectionPtr = subscriptionContextWeakPtr_.lock()) {
        if (feed_.get().notified_.contains(connectionPtr.get()))
            return;

        feed_.get().notified_.insert(connectionPtr.get());

        if (connectionPtr->apiSubversion() < 2u) {
            connectionPtr->send(std::shared_ptr<std::string>(allVersionMsgs, &allVersionMsgs->v1));
        } else {
            connectionPtr->send(std::shared_ptr<std::string>(allVersionMsgs, &allVersionMsgs->v2));
        }
    }
}

void
ProposedTransactionFeed::sub(SubscriberSharedPtr const& subscriber)
{
    auto const added =
        signal_.connectTrackableSlot(subscriber, ProposedTransactionSlot(*this, subscriber));

    if (added) {
        LOG(logger_.info()) << subscriber->tag() << "Subscribed tx_proposed";
        ++subAllCount_.get();
        subscriber->onDisconnect([this](SubscriberPtr connection) { unsubInternal(connection); });
    }
}

void
ProposedTransactionFeed::sub(
    ripple::AccountID const& account,
    SubscriberSharedPtr const& subscriber
)
{
    auto const added = accountSignal_.connectTrackableSlot(
        subscriber, account, ProposedTransactionSlot(*this, subscriber)
    );

    if (added) {
        LOG(logger_.info()) << subscriber->tag() << "Subscribed accounts_proposed " << account;
        ++subAccountCount_.get();
        subscriber->onDisconnect([this, account](SubscriberPtr connection) {
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
ProposedTransactionFeed::unsub(
    ripple::AccountID const& account,
    SubscriberSharedPtr const& subscriber
)
{
    unsubInternal(account, subscriber.get());
}

void
ProposedTransactionFeed::pub(boost::json::object const& receivedTxJson)
{
    // v2: rename "transaction" → "tx_json", move "hash" to top level
    auto const v2Json = [&]() {
        boost::json::object v2Json = receivedTxJson;
        if (v2Json.contains(JS(transaction))) {
            boost::json::value txVal = v2Json.at(JS(transaction));
            v2Json.erase(JS(transaction));
            if (txVal.is_object()) {
                auto& txObj = txVal.as_object();
                if (txObj.contains(JS(hash))) {
                    v2Json[JS(hash)] = txObj.at(JS(hash));
                    txObj.erase(JS(hash));
                }
            }
            v2Json[JS(tx_json)] = std::move(txVal);
        }
        return v2Json;
    }();

    auto const allVersionMsgs = std::make_shared<AllVersionMsgsType>(
        // v1: forward as-is (rippled sends "transaction" key)
        boost::json::serialize(receivedTxJson),
        boost::json::serialize(v2Json)
    );

    auto const transaction = receivedTxJson.at(JS(transaction)).as_object();
    auto const accounts = rpc::getAccountsFromTransaction(transaction);
    auto affectedAccounts =
        std::unordered_set<ripple::AccountID>(accounts.cbegin(), accounts.cend());

    [[maybe_unused]] auto task =
        strand_.execute([this, allVersionMsgs, affectedAccounts = std::move(affectedAccounts)]() {
            notified_.clear();
            signal_.emit(allVersionMsgs);
            // Prevent the same connection from receiving the same message twice if it is subscribed
            // to multiple accounts. However, if the same connection subscribes both stream and
            // account, it will still receive the message twice. notified_ can be cleared before
            // signal_ emit to improve this, but let's keep it as is for now, since rippled acts
            // like this.
            notified_.clear();
            for (auto const& account : affectedAccounts)
                accountSignal_.emit(account, allVersionMsgs);
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
