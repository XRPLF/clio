//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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
#include <rpc/RPCHelpers.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

namespace RPC {
template <typename SubscriptionManagerType>
class BaseSubscribeHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<SubscriptionManagerType> subscriptions_;

public:
    struct Output
    {
        // response of stream "ledger"
        // TODO: use better type than json, this type will be used in the stream as well
        std::optional<boost::json::object> ledger;
        // books returns nothing by default, if snapshot is true, it returns offers
        // TODO: use better type than json
        std::optional<boost::json::array> offers;
    };

    struct OrderBook
    {
        ripple::Book book;
        std::optional<std::string> taker;
        bool snapshot = false;
        bool both = false;
    };

    struct Input
    {
        std::optional<std::vector<std::string>> accounts;
        std::optional<std::vector<std::string>> streams;
        std::optional<std::vector<std::string>> accountsProposed;
        std::optional<std::vector<OrderBook>> books;
    };

    using Result = HandlerReturnType<Output>;

    BaseSubscribeHandler(
        std::shared_ptr<BackendInterface> const& sharedPtrBackend,
        std::shared_ptr<SubscriptionManagerType> const& subscriptions)
        : sharedPtrBackend_(sharedPtrBackend), subscriptions_(subscriptions)
    {
    }

    RpcSpecConstRef
    spec() const
    {
        static auto const booksValidator =
            validation::CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
                if (!value.is_array())
                    return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotArray"}};

                for (auto const& book : value.as_array())
                {
                    if (!book.is_object())
                        return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "ItemNotObject"}};

                    if (book.as_object().contains("both") && !book.as_object().at("both").is_bool())
                        return Error{Status{RippledError::rpcINVALID_PARAMS, "bothNotBool"}};

                    if (book.as_object().contains("snapshot") && !book.as_object().at("snapshot").is_bool())
                        return Error{Status{RippledError::rpcINVALID_PARAMS, "snapshotNotBool"}};

                    if (book.as_object().contains("taker"))
                        if (auto const err = validation::AccountValidator.verify(book.as_object(), "taker"); !err)
                            return err;

                    auto const parsedBook = parseBook(book.as_object());
                    if (auto const status = std::get_if<Status>(&parsedBook))
                        return Error(*status);
                }

                return MaybeError{};
            }};

        static auto const rpcSpec = RpcSpec{
            {JS(streams), validation::SubscribeStreamValidator},
            {JS(accounts), validation::SubscribeAccountsValidator},
            {JS(accounts_proposed), validation::SubscribeAccountsValidator},
            {JS(books), booksValidator},
        };

        return rpcSpec;
    }

    Result
    process(Input input, Context const& ctx) const
    {
        auto output = Output{};

        if (input.streams)
        {
            auto const ledger = subscribeToStreams(ctx.yield, *(input.streams), ctx.session);
            if (!ledger.empty())
                output.ledger = ledger;
        }

        if (input.accounts)
            subscribeToAccounts(*(input.accounts), ctx.session);

        if (input.accountsProposed)
            subscribeToAccountsProposed(*(input.accountsProposed), ctx.session);

        if (input.books)
        {
            auto const offers = subscribeToBooks(*(input.books), ctx.session, ctx.yield);
            if (!offers.empty())
                output.offers = offers;
        };

        return output;
    }

private:
    boost::json::object
    subscribeToStreams(
        boost::asio::yield_context& yield,
        std::vector<std::string> const& streams,
        std::shared_ptr<Server::ConnectionBase> const& session) const
    {
        auto response = boost::json::object{};

        for (auto const& stream : streams)
        {
            if (stream == "ledger")
                response = subscriptions_->subLedger(yield, session);
            else if (stream == "transactions")
                subscriptions_->subTransactions(session);
            else if (stream == "transactions_proposed")
                subscriptions_->subProposedTransactions(session);
            else if (stream == "validations")
                subscriptions_->subValidation(session);
            else if (stream == "manifests")
                subscriptions_->subManifest(session);
            else if (stream == "book_changes")
                subscriptions_->subBookChanges(session);
        }

        return response;
    }

    void
    subscribeToAccounts(
        std::vector<std::string> const& accounts,
        std::shared_ptr<Server::ConnectionBase> const& session) const
    {
        for (auto const& account : accounts)
        {
            auto const accountID = accountFromStringStrict(account);
            subscriptions_->subAccount(*accountID, session);
        }
    }

    void
    subscribeToAccountsProposed(
        std::vector<std::string> const& accounts,
        std::shared_ptr<Server::ConnectionBase> const& session) const
    {
        for (auto const& account : accounts)
        {
            auto const accountID = accountFromStringStrict(account);
            subscriptions_->subProposedAccount(*accountID, session);
        }
    }

    boost::json::array
    subscribeToBooks(
        std::vector<OrderBook> const& books,
        std::shared_ptr<Server::ConnectionBase> const& session,
        boost::asio::yield_context& yield) const
    {
        static auto constexpr fetchLimit = 200;

        boost::json::array snapshots;
        std::optional<Backend::LedgerRange> rng;

        for (auto const& internalBook : books)
        {
            if (internalBook.snapshot)
            {
                if (!rng)
                    rng = sharedPtrBackend_->fetchLedgerRange();

                auto const getOrderBook = [&](auto const& book) {
                    auto const bookBase = getBookBase(book);
                    auto const [offers, _] =
                        sharedPtrBackend_->fetchBookOffers(bookBase, rng->maxSequence, fetchLimit, yield);

                    // the taker is not really uesed, same issue with
                    // https://github.com/XRPLF/xrpl-dev-portal/issues/1818
                    auto const takerID =
                        internalBook.taker ? accountFromStringStrict(*(internalBook.taker)) : beast::zero;

                    auto const orderBook =
                        postProcessOrderBook(offers, book, *takerID, *sharedPtrBackend_, rng->maxSequence, yield);
                    std::copy(orderBook.begin(), orderBook.end(), std::back_inserter(snapshots));
                };

                getOrderBook(internalBook.book);

                if (internalBook.both)
                    getOrderBook(ripple::reversed(internalBook.book));
            }

            subscriptions_->subBook(internalBook.book, session);

            if (internalBook.both)
                subscriptions_->subBook(ripple::reversed(internalBook.book), session);
        }

        return snapshots;
    }

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output)
    {
        jv = output.ledger ? *(output.ledger) : boost::json::object();

        if (output.offers)
            jv.as_object().emplace(JS(offers), *(output.offers));
    }

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv)
    {
        auto input = Input{};
        auto const& jsonObject = jv.as_object();

        if (auto const& streams = jsonObject.find(JS(streams)); streams != jsonObject.end())
        {
            input.streams = std::vector<std::string>();
            for (auto const& stream : streams->value().as_array())
                input.streams->push_back(stream.as_string().c_str());
        }

        if (auto const& accounts = jsonObject.find(JS(accounts)); accounts != jsonObject.end())
        {
            input.accounts = std::vector<std::string>();
            for (auto const& account : accounts->value().as_array())
                input.accounts->push_back(account.as_string().c_str());
        }

        if (auto const& accountsProposed = jsonObject.find(JS(accounts_proposed)); accountsProposed != jsonObject.end())
        {
            input.accountsProposed = std::vector<std::string>();
            for (auto const& account : accountsProposed->value().as_array())
                input.accountsProposed->push_back(account.as_string().c_str());
        }

        if (auto const& books = jsonObject.find(JS(books)); books != jsonObject.end())
        {
            input.books = std::vector<OrderBook>();
            for (auto const& book : books->value().as_array())
            {
                auto internalBook = OrderBook{};
                auto const& bookObject = book.as_object();

                if (auto const& taker = bookObject.find(JS(taker)); taker != bookObject.end())
                    internalBook.taker = taker->value().as_string().c_str();

                if (auto const& both = bookObject.find(JS(both)); both != bookObject.end())
                    internalBook.both = both->value().as_bool();

                if (auto const& snapshot = bookObject.find(JS(snapshot)); snapshot != bookObject.end())
                    internalBook.snapshot = snapshot->value().as_bool();

                auto const parsedBookMaybe = parseBook(book.as_object());
                internalBook.book = std::get<ripple::Book>(parsedBookMaybe);
                input.books->push_back(internalBook);
            }
        }

        return input;
    }
};

/**
 * @brief The subscribe method requests periodic notifications from the server when certain events happen.
 *
 * For more details see: https://xrpl.org/subscribe.html
 */
using SubscribeHandler = BaseSubscribeHandler<SubscriptionManager>;

}  // namespace RPC
