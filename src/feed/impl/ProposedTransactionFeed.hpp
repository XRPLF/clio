//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "feed/Types.hpp"
#include "feed/impl/TrackableSignal.hpp"
#include "feed/impl/TrackableSignalMap.hpp"
#include "feed/impl/Util.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json/object.hpp>
#include <fmt/core.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/Book.h>
#include <ripple/protocol/LedgerHeader.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>

namespace feed::impl {

/**
 * @brief Feed that publishes the Proposed Transactions.
 * @note Be aware that the Clio only forwards this stream, not respect api_version.
 */
class ProposedTransactionFeed {
    util::Logger logger_{"Subscriptions"};

    std::unordered_set<SubscriberPtr>
        notified_;  // Used by slots to prevent double notifications if tx contains multiple subscribed accounts
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    std::reference_wrapper<util::prometheus::GaugeInt> subAllCount_;
    std::reference_wrapper<util::prometheus::GaugeInt> subAccountCount_;
    std::reference_wrapper<util::prometheus::CounterInt> pubCount_;

    TrackableSignalMap<ripple::AccountID, Subscriber, std::shared_ptr<std::string>> accountSignal_;
    TrackableSignal<Subscriber, std::shared_ptr<std::string>> signal_;

public:
    /**
     * @brief Construct a Proposed Transaction Feed object.
     * @param ioContext The actual publish will be called in the strand of this.
     */
    ProposedTransactionFeed(boost::asio::io_context& ioContext)
        : strand_(boost::asio::make_strand(ioContext))
        , subAllCount_(getSubscriptionsGaugeInt("tx_proposed"))
        , subAccountCount_(getSubscriptionsGaugeInt("account_proposed"))
        , pubCount_(getPublishedMessagesCounterInt("tx_proposed"))
    {
    }

    /**
     * @brief Subscribe to the proposed transaction feed.
     * @param subscriber
     */
    void
    sub(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Subscribe to the proposed transaction feed, only receive the feed when particular account is affected.
     * @param subscriber
     * @param account The account to watch.
     */
    void
    sub(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the proposed transaction feed.
     * @param subscriber
     */
    void
    unsub(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe to the proposed transaction feed for particular account.
     * @param subscriber
     * @param account The account to unsubscribe.
     */
    void
    unsub(ripple::AccountID const& account, SubscriberSharedPtr const& subscriber);

    /**
     * @brief Publishes the proposed transaction feed.
     * @param receivedTxJson The proposed transaction json.
     */
    void
    pub(boost::json::object const& receivedTxJson);

    /**
     * @brief Get the number of subscribers of the proposed transaction feed.
     */
    std::uint64_t
    transactionSubcount() const;

    /**
     * @brief Get the number of accounts subscribers.
     */
    std::uint64_t
    accountSubCount() const;

private:
    void
    unsubInternal(SubscriberPtr subscriber);

    void
    unsubInternal(ripple::AccountID const& account, SubscriberPtr subscriber);
};
}  // namespace feed::impl
