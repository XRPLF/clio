#pragma once

#include "feed/SubscriptionManagerInterface.hpp"
#include "feed/Types.hpp"
#include "rpc/common/SpecBackend.hpp"
#include "rpc/common/Types.hpp"

#include <rpcspec/handlers/unsubscribe/Types.hpp>
#include <xrpl/protocol/AccountID.h>

#include <expected>
#include <memory>
#include <vector>

namespace rpc {

/**
 * @brief Handles the `unsubscribe` command which is used to disconnect a subscriber from a feed.
 * The unsubscribe command tells the server to stop sending messages for a particular subscription
 * or set of subscriptions.
 *
 * For more details see: https://xrpl.org/unsubscribe.html
 */

class UnsubscribeHandler : public rpc::HandlerFor<rpc::spec::handlers::unsubscribe::Input> {
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions_;

public:
    /**
     * @brief A struct to hold one order book
     */
    using OrderBook = rpc::spec::handlers::unsubscribe::OrderBook;

    /**
     * @brief A subscribable stream type.
     */
    using StreamType = rpc::spec::handlers::unsubscribe::StreamType;

    using Output = VoidOutput;
    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new BaseUnsubscribeHandler object
     *
     * @param subscriptions The subscription manager to use
     */
    UnsubscribeHandler(std::shared_ptr<feed::SubscriptionManagerInterface> const& subscriptions);

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
        std::vector<StreamType> const& streams,
        feed::SubscriberSharedPtr const& session
    ) const;

    void
    unsubscribeFromAccounts(
        std::vector<xrpl::AccountID> const& accounts,
        feed::SubscriberSharedPtr const& session
    ) const;

    void
    unsubscribeFromProposedAccounts(
        std::vector<xrpl::AccountID> const& accountsProposed,
        feed::SubscriberSharedPtr const& session
    ) const;

    void
    unsubscribeFromBooks(
        std::vector<OrderBook> const& books,
        feed::SubscriberSharedPtr const& session
    ) const;
};

}  // namespace rpc
