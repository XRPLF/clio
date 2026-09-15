#include "rpc/handlers/Subscribe.hpp"

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "feed/Types.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/handlers/subscribe/Types.hpp>
#include <xrpl/beast/utility/Zero.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/jss.h>

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace rpc {

SubscribeHandler::SubscribeHandler(
    std::shared_ptr<BackendInterface> sharedPtrBackend,
    std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter,
    std::shared_ptr<feed::SubscriptionManagerInterface> const& subscriptions
)
    : sharedPtrBackend_(std::move(sharedPtrBackend))
    , amendmentCenter_(amendmentCenter)
    , subscriptions_(subscriptions)
{
}

SubscribeHandler::Result
SubscribeHandler::process(Input const& input, Context const& ctx) const
{
    auto output = Output{};

    // Mimic rippled. No matter what the request is, the api version changes for the whole session
    ctx.session->setApiSubversion(ctx.apiVersion);

    if (input.streams) {
        auto const ledger = subscribeToStreams(ctx.yield, *(input.streams), ctx.session);
        if (!ledger.empty())
            output.ledger = ledger;
    }

    if (input.accounts)
        subscribeToAccounts(*(input.accounts), ctx.session);

    if (input.accountsProposed)
        subscribeToAccountsProposed(*(input.accountsProposed), ctx.session);

    if (input.books)
        subscribeToBooks(*(input.books), ctx.session, ctx.yield, output);

    return output;
}

boost::json::object
SubscribeHandler::subscribeToStreams(
    boost::asio::yield_context yield,
    std::vector<StreamType> const& streams,
    feed::SubscriberSharedPtr const& session
) const
{
    auto response = boost::json::object{};

    for (auto const& stream : streams) {
        switch (stream) {
            case StreamType::Ledger:
                response = subscriptions_->subLedger(yield, session);
                break;
            case StreamType::Transactions:
                subscriptions_->subTransactions(session);
                break;
            case StreamType::TransactionsProposed:
                subscriptions_->subProposedTransactions(session);
                break;
            case StreamType::Validations:
                subscriptions_->subValidation(session);
                break;
            case StreamType::Manifests:
                subscriptions_->subManifest(session);
                break;
            case StreamType::BookChanges:
                subscriptions_->subBookChanges(session);
                break;
            case StreamType::Server:
            case StreamType::PeerStatus:
            case StreamType::Consensus:
                // Not served by Clio.
                break;
        }
    }

    return response;
}

void
SubscribeHandler::subscribeToAccountsProposed(
    std::vector<xrpl::AccountID> const& accounts,
    feed::SubscriberSharedPtr const& session
) const
{
    for (auto const& account : accounts)
        subscriptions_->subProposedAccount(account, session);
}

void
SubscribeHandler::subscribeToAccounts(
    std::vector<xrpl::AccountID> const& accounts,
    feed::SubscriberSharedPtr const& session
) const
{
    for (auto const& account : accounts)
        subscriptions_->subAccount(account, session);
}

void
SubscribeHandler::subscribeToBooks(
    std::vector<OrderBook> const& books,
    feed::SubscriberSharedPtr const& session,
    boost::asio::yield_context yield,
    Output& output
) const
{
    static constexpr auto kFetchLimit = 200;

    std::optional<data::LedgerRange> rng;

    for (auto const& internalBook : books) {
        if (internalBook.snapshot) {
            if (!rng) {
                rng = sharedPtrBackend_->fetchLedgerRange();
                ASSERT(rng.has_value(), "Subscribe's ledger range must be available");
            }

            auto const getOrderBook = [&](auto const& book, auto& snapshots) {
                auto const bookBase = getBookBase(book);
                auto const [offers, _] = sharedPtrBackend_->fetchBookOffers(
                    bookBase, rng->maxSequence, kFetchLimit, yield
                );

                // the taker is not really used, same issue with
                // https://github.com/XRPLF/xrpl-dev-portal/issues/1818
                auto const takerID = internalBook.taker
                    ? accountFromStringStrict(*(internalBook.taker))
                    : beast::kZero;

                auto const orderBook = postProcessOrderBook(
                    offers,
                    book,
                    *takerID,
                    *sharedPtrBackend_,
                    *amendmentCenter_,
                    rng->maxSequence,
                    yield
                );
                std::copy(orderBook.begin(), orderBook.end(), std::back_inserter(snapshots));
            };

            if (internalBook.both) {
                if (!output.bids)
                    output.bids = boost::json::array();
                if (!output.asks)
                    output.asks = boost::json::array();
                getOrderBook(internalBook.book, *(output.bids));
                getOrderBook(xrpl::reversed(internalBook.book), *(output.asks));
            } else {
                if (!output.offers)
                    output.offers = boost::json::array();
                getOrderBook(internalBook.book, *(output.offers));
            }
        }

        subscriptions_->subBook(internalBook.book, session);

        if (internalBook.both)
            subscriptions_->subBook(xrpl::reversed(internalBook.book), session);
    }
}

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    SubscribeHandler::Output const& output
)
{
    jv = output.ledger ? *(output.ledger) : boost::json::object();

    if (output.offers)
        jv.as_object().emplace(JS(offers), *(output.offers));
    if (output.asks)
        jv.as_object().emplace(JS(asks), *(output.asks));
    if (output.bids)
        jv.as_object().emplace(JS(bids), *(output.bids));
}

}  // namespace rpc
