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
class BaseUnsubscribeHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<SubscriptionManagerType> subscriptions_;

public:
    struct OrderBook
    {
        ripple::Book book;
        bool both = false;
    };

    struct Input
    {
        std::optional<std::vector<std::string>> accounts;
        std::optional<std::vector<std::string>> streams;
        std::optional<std::vector<std::string>> accountsProposed;
        std::optional<std::vector<OrderBook>> books;
    };

    using Output = VoidOutput;
    using Result = HandlerReturnType<Output>;

    BaseUnsubscribeHandler(
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

private:
    void
    unsubscribeFromStreams(
        std::vector<std::string> const& streams,
        std::shared_ptr<Server::ConnectionBase> const& session) const
    {
        for (auto const& stream : streams)
        {
            if (stream == "ledger")
                subscriptions_->unsubLedger(session);
            else if (stream == "transactions")
                subscriptions_->unsubTransactions(session);
            else if (stream == "transactions_proposed")
                subscriptions_->unsubProposedTransactions(session);
            else if (stream == "validations")
                subscriptions_->unsubValidation(session);
            else if (stream == "manifests")
                subscriptions_->unsubManifest(session);
            else if (stream == "book_changes")
                subscriptions_->unsubBookChanges(session);
            else
                assert(false);
        }
    }

    void
    unsubscribeFromAccounts(std::vector<std::string> accounts, std::shared_ptr<Server::ConnectionBase> const& session)
        const
    {
        for (auto const& account : accounts)
        {
            auto const accountID = accountFromStringStrict(account);
            subscriptions_->unsubAccount(*accountID, session);
        }
    }

    void
    unsubscribeFromProposedAccounts(
        std::vector<std::string> accountsProposed,
        std::shared_ptr<Server::ConnectionBase> const& session) const
    {
        for (auto const& account : accountsProposed)
        {
            auto const accountID = accountFromStringStrict(account);
            subscriptions_->unsubProposedAccount(*accountID, session);
        }
    }

    void
    unsubscribeFromBooks(std::vector<OrderBook> const& books, std::shared_ptr<Server::ConnectionBase> const& session)
        const
    {
        for (auto const& orderBook : books)
        {
            subscriptions_->unsubBook(orderBook.book, session);

            if (orderBook.both)
                subscriptions_->unsubBook(ripple::reversed(orderBook.book), session);
        }
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

                if (auto const& both = bookObject.find(JS(both)); both != bookObject.end())
                    internalBook.both = both->value().as_bool();

                auto const parsedBookMaybe = parseBook(book.as_object());
                internalBook.book = std::get<ripple::Book>(parsedBookMaybe);
                input.books->push_back(internalBook);
            }
        }

        return input;
    }
};

/**
 * @brief The unsubscribe command tells the server to stop sending messages for a particular subscription or set of
 * subscriptions.
 *
 * For more details see: https://xrpl.org/unsubscribe.html
 */
using UnsubscribeHandler = BaseUnsubscribeHandler<SubscriptionManager>;

}  // namespace RPC
