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

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Checkers.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <ripple/beast/utility/Zero.h>
#include <ripple/protocol/Book.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace feed {
class SubscriptionManager;
}  // namespace feed

namespace rpc {

/**
 * @brief Contains functionality for handling the `subscribe` command
 *
 * @tparam SubscriptionManagerType The type of the subscription manager to use
 */
template <typename SubscriptionManagerType>
class BaseSubscribeHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<SubscriptionManagerType> subscriptions_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        // response of stream "ledger"
        // TODO: use better type than json, this type will be used in the stream as well
        std::optional<boost::json::object> ledger;
        // books returns nothing by default, if snapshot is true and both is false, offers go to offers list
        // TODO: use better type than json
        std::optional<boost::json::array> offers;
        // if snapshot is true and both is true, reversed book' offers go to asks list
        std::optional<boost::json::array> asks;
        // if snapshot is true and both is true, original book' offers go to bids list
        std::optional<boost::json::array> bids;
    };

    /**
     * @brief A struct to hold the data for one order book
     */
    struct OrderBook {
        ripple::Book book;
        std::optional<std::string> taker;
        bool snapshot = false;
        bool both = false;
    };

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::optional<std::vector<std::string>> accounts;
        std::optional<std::vector<std::string>> streams;
        std::optional<std::vector<std::string>> accountsProposed;
        std::optional<std::vector<OrderBook>> books;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new BaseSubscribeHandler object
     *
     * @param sharedPtrBackend The backend to use
     * @param subscriptions The subscription manager to use
     */
    BaseSubscribeHandler(
        std::shared_ptr<BackendInterface> const& sharedPtrBackend,
        std::shared_ptr<SubscriptionManagerType> const& subscriptions
    )
        : sharedPtrBackend_(sharedPtrBackend), subscriptions_(subscriptions)
    {
    }

    /**
     * @brief Returns the API specification for the command
     *
     * @param apiVersion The api version to return the spec for
     * @return The spec for the given apiVersion
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        static auto const booksValidator =
            validation::CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
                if (!value.is_array())
                    return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotArray"}};

                for (auto const& book : value.as_array()) {
                    if (!book.is_object())
                        return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "ItemNotObject"}};

                    if (book.as_object().contains("both") && !book.as_object().at("both").is_bool())
                        return Error{Status{RippledError::rpcINVALID_PARAMS, "bothNotBool"}};

                    if (book.as_object().contains("snapshot") && !book.as_object().at("snapshot").is_bool())
                        return Error{Status{RippledError::rpcINVALID_PARAMS, "snapshotNotBool"}};

                    if (book.as_object().contains("taker")) {
                        if (auto err = meta::WithCustomError(
                                           validation::AccountValidator,
                                           Status{RippledError::rpcBAD_ISSUER, "Issuer account malformed."}
                            )
                                           .verify(book.as_object(), "taker");
                            !err)
                            return err;
                    }

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
            {"user", check::Deprecated{}},
            {JS(password), check::Deprecated{}},
            {JS(rt_accounts), check::Deprecated{}}
        };

        return rpcSpec;
    }

    /**
     * @brief Process the Subscribe command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    Result
    process(Input input, Context const& ctx) const
    {
        auto output = Output{};

        // Mimic rippled. No matter what the request is, the api version changes for the session
        ctx.session->apiSubVersion = ctx.apiVersion;

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

private:
    boost::json::object
    subscribeToStreams(
        boost::asio::yield_context yield,
        std::vector<std::string> const& streams,
        std::shared_ptr<web::ConnectionBase> const& session
    ) const
    {
        auto response = boost::json::object{};

        for (auto const& stream : streams) {
            if (stream == "ledger") {
                response = subscriptions_->subLedger(yield, session);
            } else if (stream == "transactions") {
                subscriptions_->subTransactions(session);
            } else if (stream == "transactions_proposed") {
                subscriptions_->subProposedTransactions(session);
            } else if (stream == "validations") {
                subscriptions_->subValidation(session);
            } else if (stream == "manifests") {
                subscriptions_->subManifest(session);
            } else if (stream == "book_changes") {
                subscriptions_->subBookChanges(session);
            }
        }

        return response;
    }

    void
    subscribeToAccounts(std::vector<std::string> const& accounts, std::shared_ptr<web::ConnectionBase> const& session)
        const
    {
        for (auto const& account : accounts) {
            auto const accountID = accountFromStringStrict(account);
            subscriptions_->subAccount(*accountID, session);
        }
    }

    void
    subscribeToAccountsProposed(
        std::vector<std::string> const& accounts,
        std::shared_ptr<web::ConnectionBase> const& session
    ) const
    {
        for (auto const& account : accounts) {
            auto const accountID = accountFromStringStrict(account);
            subscriptions_->subProposedAccount(*accountID, session);
        }
    }

    void
    subscribeToBooks(
        std::vector<OrderBook> const& books,
        std::shared_ptr<web::ConnectionBase> const& session,
        boost::asio::yield_context yield,
        Output& output
    ) const
    {
        static auto constexpr fetchLimit = 200;

        std::optional<data::LedgerRange> rng;

        for (auto const& internalBook : books) {
            if (internalBook.snapshot) {
                if (!rng)
                    rng = sharedPtrBackend_->fetchLedgerRange();

                auto const getOrderBook = [&](auto const& book, auto& snapshots) {
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

                if (internalBook.both) {
                    if (!output.bids)
                        output.bids = boost::json::array();
                    if (!output.asks)
                        output.asks = boost::json::array();
                    getOrderBook(internalBook.book, *(output.bids));
                    getOrderBook(ripple::reversed(internalBook.book), *(output.asks));
                } else {
                    if (!output.offers)
                        output.offers = boost::json::array();
                    getOrderBook(internalBook.book, *(output.offers));
                }
            }

            subscriptions_->subBook(internalBook.book, session);

            if (internalBook.both)
                subscriptions_->subBook(ripple::reversed(internalBook.book), session);
        }
    }

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output)
    {
        jv = output.ledger ? *(output.ledger) : boost::json::object();

        if (output.offers)
            jv.as_object().emplace(JS(offers), *(output.offers));
        if (output.asks)
            jv.as_object().emplace(JS(asks), *(output.asks));
        if (output.bids)
            jv.as_object().emplace(JS(bids), *(output.bids));
    }

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv)
    {
        auto input = Input{};
        auto const& jsonObject = jv.as_object();

        if (auto const& streams = jsonObject.find(JS(streams)); streams != jsonObject.end()) {
            input.streams = std::vector<std::string>();
            for (auto const& stream : streams->value().as_array())
                input.streams->push_back(boost::json::value_to<std::string>(stream));
        }

        if (auto const& accounts = jsonObject.find(JS(accounts)); accounts != jsonObject.end()) {
            input.accounts = std::vector<std::string>();
            for (auto const& account : accounts->value().as_array())
                input.accounts->push_back(boost::json::value_to<std::string>(account));
        }

        if (auto const& accountsProposed = jsonObject.find(JS(accounts_proposed));
            accountsProposed != jsonObject.end()) {
            input.accountsProposed = std::vector<std::string>();
            for (auto const& account : accountsProposed->value().as_array())
                input.accountsProposed->push_back(boost::json::value_to<std::string>(account));
        }

        if (auto const& books = jsonObject.find(JS(books)); books != jsonObject.end()) {
            input.books = std::vector<OrderBook>();
            for (auto const& book : books->value().as_array()) {
                auto internalBook = OrderBook{};
                auto const& bookObject = book.as_object();

                if (auto const& taker = bookObject.find(JS(taker)); taker != bookObject.end())
                    internalBook.taker = boost::json::value_to<std::string>(taker->value());

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
using SubscribeHandler = BaseSubscribeHandler<feed::SubscriptionManager>;

}  // namespace rpc
