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

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rpc {

/**
 * @brief Handles the `unsubscribe` command which is used to disconnect a subscriber from a feed.
 * The unsubscribe command tells the server to stop sending messages for a particular subscription or set of
 * subscriptions.
 *
 * For more details see: https://xrpl.org/unsubscribe.html
 */

class UnsubscribeHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions_;

public:
    /**
     * @brief A struct to hold one order book
     */
    struct OrderBook {
        ripple::Book book;
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

    using Output = VoidOutput;
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new BaseUnsubscribeHandler object
     *
     * @param sharedPtrBackend The backend to use
     * @param subscriptions The subscription manager to use
     */
    UnsubscribeHandler(
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
     * @brief Process the Unsubscribe command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    Result
    process(Input input, Context const& ctx) const;

private:
    void
    unsubscribeFromStreams(std::vector<std::string> const& streams, std::shared_ptr<web::ConnectionBase> const& session)
        const;

    void
    unsubscribeFromAccounts(std::vector<std::string> accounts, std::shared_ptr<web::ConnectionBase> const& session)
        const;

    void
    unsubscribeFromProposedAccounts(
        std::vector<std::string> accountsProposed,
        std::shared_ptr<web::ConnectionBase> const& session
    ) const;

    void
    unsubscribeFromBooks(std::vector<OrderBook> const& books, std::shared_ptr<web::ConnectionBase> const& session)
        const;

    /**
     * @brief Convert a JSON object to an Input
     *
     * @param jv The JSON object to convert
     * @return The Input object
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};

}  // namespace rpc
