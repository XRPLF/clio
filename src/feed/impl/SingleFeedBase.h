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

#include "feed/Types.h"
#include "feed/impl/TrackableSignal.h"
#include "util/log/Logger.h"
#include "util/prometheus/Gauge.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace feed::impl {

/**
 * @brief Base class for single feed.
 */
class SingleFeedBase {
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    std::reference_wrapper<util::prometheus::GaugeInt> subCount_;
    TrackableSignal<Subscriber, std::shared_ptr<std::string> const&> signal_;
    util::Logger logger_{"Subscriptions"};
    std::string name_;

public:
    /**
     * @brief Construct a new Single Feed Base object
     * @param ioContext The actual publish will be called in the strand of this.
     * @param name The promethues counter name of the feed.
     */
    SingleFeedBase(boost::asio::io_context& ioContext, std::string const& name);

    /**
     * @brief Subscribe the feed.
     */
    void
    sub(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe the feed.
     */
    void
    unsub(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Publishes the feed in strand.
     * @param msg The message.
     */
    void
    pub(std::string msg) const;

    /**
     * @brief Get the count of subscribers.
     */
    std::uint64_t
    count() const;

private:
    void
    unsubInternal(SubscriberPtr subscriber);
};
}  // namespace feed::impl
