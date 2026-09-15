#include "rpc/handlers/Unsubscribe.hpp"

#include "feed/SubscriptionManagerInterface.hpp"
#include "feed/Types.hpp"
#include "rpc/common/Types.hpp"

#include <rpcspec/handlers/unsubscribe/Types.hpp>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>

#include <expected>
#include <memory>
#include <vector>

namespace rpc {

UnsubscribeHandler::UnsubscribeHandler(
    std::shared_ptr<feed::SubscriptionManagerInterface> const& subscriptions
)
    : subscriptions_(subscriptions)
{
}

UnsubscribeHandler::Result
UnsubscribeHandler::process(Input const& input, Context const& ctx) const
{
    if (input.streams)
        unsubscribeFromStreams(*(input.streams), ctx.session);

    if (input.accounts)
        unsubscribeFromAccounts(*(input.accounts), ctx.session);

    if (input.accountsProposed)
        unsubscribeFromProposedAccounts(*(input.accountsProposed), ctx.session);

    if (input.books)
        unsubscribeFromBooks(*(input.books), ctx.session);

    return Output{};
}

void
UnsubscribeHandler::unsubscribeFromStreams(
    std::vector<StreamType> const& streams,
    feed::SubscriberSharedPtr const& session
) const
{
    for (auto const& stream : streams) {
        switch (stream) {
            case StreamType::Ledger:
                subscriptions_->unsubLedger(session);
                break;
            case StreamType::Transactions:
                subscriptions_->unsubTransactions(session);
                break;
            case StreamType::TransactionsProposed:
                subscriptions_->unsubProposedTransactions(session);
                break;
            case StreamType::Validations:
                subscriptions_->unsubValidation(session);
                break;
            case StreamType::Manifests:
                subscriptions_->unsubManifest(session);
                break;
            case StreamType::BookChanges:
                subscriptions_->unsubBookChanges(session);
                break;
            case StreamType::Server:
            case StreamType::PeerStatus:
            case StreamType::Consensus:
                // Not served by Clio.
                break;
        }
    }
}

void
UnsubscribeHandler::unsubscribeFromAccounts(
    std::vector<xrpl::AccountID> const& accounts,
    feed::SubscriberSharedPtr const& session
) const
{
    for (auto const& account : accounts)
        subscriptions_->unsubAccount(account, session);
}

void
UnsubscribeHandler::unsubscribeFromProposedAccounts(
    std::vector<xrpl::AccountID> const& accountsProposed,
    feed::SubscriberSharedPtr const& session
) const
{
    for (auto const& account : accountsProposed)
        subscriptions_->unsubProposedAccount(account, session);
}

void
UnsubscribeHandler::unsubscribeFromBooks(
    std::vector<OrderBook> const& books,
    feed::SubscriberSharedPtr const& session
) const
{
    for (auto const& orderBook : books) {
        subscriptions_->unsubBook(orderBook.book, session);

        if (orderBook.both)
            subscriptions_->unsubBook(xrpl::reversed(orderBook.book), session);
    }
}

}  // namespace rpc
