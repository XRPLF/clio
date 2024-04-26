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
#include "feed/SubscriptionManagerInterface.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"

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
#include <vector>

namespace feed {
class SubscriptionManager;
}  // namespace feed

namespace rpc {

/**
 * @brief Contains functionality for handling the `subscribe` command.
 * The subscribe method requests periodic notifications from the server when certain events happen.
 *
 * For more details see: https://xrpl.org/subscribe.html
 */

class SubscribeHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions_;

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
    SubscribeHandler(
        std::shared_ptr<BackendInterface> const& sharedPtrBackend,
        std::shared_ptr<feed::SubscriptionManagerInterface> const& subscriptions
    );

    /**
     * @brief Returns the API specification for the command
     *
     * @param apiVersion The api version to return the spec for
     * @return The spec for the given apiVersion
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion);

    /**
     * @brief Process the Subscribe command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    Result
    process(Input input, Context const& ctx) const;

private:
    boost::json::object
    subscribeToStreams(
        boost::asio::yield_context yield,
        std::vector<std::string> const& streams,
        std::shared_ptr<web::ConnectionBase> const& session,
        std::uint32_t apiVersion
    ) const;

    void
    subscribeToAccounts(
        std::vector<std::string> const& accounts,
        std::shared_ptr<web::ConnectionBase> const& session,
        std::uint32_t apiVersion
    ) const;

    void
    subscribeToAccountsProposed(
        std::vector<std::string> const& accounts,
        std::shared_ptr<web::ConnectionBase> const& session
    ) const;

    void
    subscribeToBooks(
        std::vector<OrderBook> const& books,
        std::shared_ptr<web::ConnectionBase> const& session,
        boost::asio::yield_context yield,
        uint32_t apiVersion,
        Output& output
    ) const;

    /**
     * @brief Convert output to json value
     *
     * @param jv The json value to convert to
     * @param output The output to convert from
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    /**
     * @brief Convert json value to input
     *
     * @param jv The json value to convert from
     * @return The input to convert to
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};

}  // namespace rpc
