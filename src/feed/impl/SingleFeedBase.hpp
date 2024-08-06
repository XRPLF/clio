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
#include "feed/impl/Util.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Gauge.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace feed::impl {

/**
 * @brief Base class for single feed.
 *
 * @tparam ExecutionContext The type of the execution context.
 */
template <typename ExecutionContext>
class SingleFeedBase {
    ExecutionContext::Strand strand_;
    std::reference_wrapper<util::prometheus::GaugeInt> subCount_;
    TrackableSignal<Subscriber, std::shared_ptr<std::string> const&> signal_;
    util::Logger logger_{"Subscriptions"};
    std::string name_;

public:
    /**
     * @brief Construct a new Single Feed Base object
     *
     * @param executorContext The actual publish will be called in the strand of this.
     * @param name The promethues counter name of the feed.
     */
    SingleFeedBase(ExecutionContext& executorContext, std::string const& name)
        : strand_(executorContext.makeStrand()), subCount_(getSubscriptionsGaugeInt(name)), name_(name)
    {
    }

    /**
     * @brief Subscribe the feed.
     *
     * @param subscriber
     */
    void
    sub(SubscriberSharedPtr const& subscriber)
    {
        auto const weakPtr = std::weak_ptr(subscriber);
        auto const added = signal_.connectTrackableSlot(subscriber, [weakPtr](std::shared_ptr<std::string> const& msg) {
            if (auto connectionPtr = weakPtr.lock())
                connectionPtr->send(msg);
        });

        if (added) {
            LOG(logger_.info()) << subscriber->tag() << "Subscribed " << name_;
            ++subCount_.get();
            subscriber->onDisconnect.connect([this](SubscriberPtr connectionDisconnecting) {
                unsubInternal(connectionDisconnecting);
            });
        };
    }

    /**
     * @brief Unsubscribe the feed.
     *
     * @param subscriber
     */
    void
    unsub(SubscriberSharedPtr const& subscriber)
    {
        unsubInternal(subscriber.get());
    }

    /**
     * @brief Publishes the feed in strand.
     * @param msg The message.
     */
    void
    pub(std::string msg) const
    {
        [[maybe_unused]] auto task = strand_.execute([this, msg = std::move(msg)]() mutable {
            auto const msgPtr = std::make_shared<std::string>(std::move(msg));
            signal_.emit(msgPtr);
        });
    }

    /**
     * @brief Get the count of subscribers.
     */
    std::uint64_t
    count() const
    {
        return subCount_.get().value();
    }

private:
    void
    unsubInternal(SubscriberPtr subscriber)
    {
        if (signal_.disconnect(subscriber)) {
            LOG(logger_.info()) << subscriber->tag() << "Unsubscribed " << name_;
            --subCount_.get();
        }
    }
};
}  // namespace feed::impl
