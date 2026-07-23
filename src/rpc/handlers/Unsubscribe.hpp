#pragma once

#include "feed/SubscriptionManagerInterface.hpp"
#include "feed/Types.hpp"
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
 * The unsubscribe command tells the server to stop sending messages for a particular subscription
 * or set of subscriptions.
 *
 * For more details see: https://xrpl.org/unsubscribe.html
 */

class UnsubscribeHandler {
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions_;

public:
    /**
     * @brief A struct to hold one order book
     */
    struct OrderBook {
        xrpl::Book book;
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
     * @param subscriptions The subscription manager to use
     */
    UnsubscribeHandler(std::shared_ptr<feed::SubscriptionManagerInterface> const& subscriptions);

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
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    void
    unsubscribeFromStreams(
        std::vector<std::string> const& streams,
        feed::SubscriberSharedPtr const& session
    ) const;

    void
    unsubscribeFromAccounts(
        std::vector<std::string> accounts,
        feed::SubscriberSharedPtr const& session
    ) const;

    void
    unsubscribeFromProposedAccounts(
        std::vector<std::string> accountsProposed,
        feed::SubscriberSharedPtr const& session
    ) const;

    void
    unsubscribeFromBooks(
        std::vector<OrderBook> const& books,
        feed::SubscriberSharedPtr const& session
    ) const;

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
