#pragma once

#include "feed/Types.hpp"
#include "feed/impl/TrackableSignal.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyStrand.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Gauge.hpp"

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
    util::async::AnyStrand strand_;
    std::reference_wrapper<util::prometheus::GaugeInt> subCount_;
    TrackableSignal<Subscriber, std::shared_ptr<std::string> const&> signal_;
    util::Logger logger_{"Subscriptions"};
    std::string name_;

public:
    /**
     * @brief Construct a new Single Feed Base object
     * @param executionCtx The actual publish will be called in the strand of this.
     * @param name The prometheus counter name of the feed.
     */
    SingleFeedBase(util::async::AnyExecutionContext& executionCtx, std::string const& name);

    /**
     * @brief Subscribe the feed.
     * @param subscriber
     */
    void
    sub(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Unsubscribe the feed.
     * @param subscriber
     */
    void
    unsub(SubscriberSharedPtr const& subscriber);

    /**
     * @brief Publishes the feed in strand.
     * @param msg The message.
     */
    void
    pub(std::string msg);

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
