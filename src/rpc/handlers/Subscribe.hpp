#pragma once

#include "data/AmendmentCenterInterface.hpp"
#include "data/BackendInterface.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "feed/Types.hpp"
#include "rpc/common/Types.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <rpcspec/HandlerFor.hpp>
#include <rpcspec/handlers/subscribe/Types.hpp>
#include <xrpl/protocol/AccountID.h>

#include <expected>
#include <memory>
#include <optional>
#include <vector>

namespace rpc {

/**
 * @brief Contains functionality for handling the `subscribe` command.
 * The subscribe method requests periodic notifications from the server when certain events happen.
 *
 * For more details see: https://xrpl.org/subscribe.html
 */

class SubscribeHandler : public rpc::spec::HandlerFor<rpc::spec::handlers::subscribe::Input> {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<data::AmendmentCenterInterface const> amendmentCenter_;
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        // response of stream "ledger"
        // TODO: use better type than json, this type will be used in the stream as well
        std::optional<boost::json::object> ledger;
        // books returns nothing by default, if snapshot is true and both is false, offers go to
        // offers list
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
    using OrderBook = rpc::spec::handlers::subscribe::OrderBook;

    /**
     * @brief A subscribable stream type.
     */
    using StreamType = rpc::spec::handlers::subscribe::StreamType;

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new BaseSubscribeHandler object
     *
     * @param sharedPtrBackend The backend to use
     * @param amendmentCenter The amendmentCenter to use
     * @param subscriptions The subscription manager to use
     */
    SubscribeHandler(
        std::shared_ptr<BackendInterface> sharedPtrBackend,
        std::shared_ptr<data::AmendmentCenterInterface const> const& amendmentCenter,
        std::shared_ptr<feed::SubscriptionManagerInterface> const& subscriptions
    );

    /**
     * @brief Process the Subscribe command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;

private:
    [[nodiscard]] boost::json::object
    subscribeToStreams(
        boost::asio::yield_context yield,
        std::vector<StreamType> const& streams,
        feed::SubscriberSharedPtr const& session
    ) const;

    void
    subscribeToAccounts(
        std::vector<xrpl::AccountID> const& accounts,
        feed::SubscriberSharedPtr const& session
    ) const;

    void
    subscribeToAccountsProposed(
        std::vector<xrpl::AccountID> const& accounts,
        feed::SubscriberSharedPtr const& session
    ) const;

    void
    subscribeToBooks(
        std::vector<OrderBook> const& books,
        feed::SubscriberSharedPtr const& session,
        boost::asio::yield_context yield,
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
};

}  // namespace rpc
