#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>

namespace feed {
class SubscriptionManagerInterface;
}  // namespace feed

namespace etl::impl {

/**
 * @brief Serializes and deduplicates subscription events received from all ETL sources.
 * @note Each producer waits for its event to be processed before reading the next event. This
 * bounds queued work by the number of sources and drains accepted work when sources stop.
 */
class SubscriptionMessageQueue {
    struct State;
    std::shared_ptr<State> state_;

public:
    using Clock = std::chrono::steady_clock;
    using Now = std::function<Clock::time_point()>;

    /**
     * @brief Limits for the cache of recently forwarded events.
     * @note Events are compared as JSON values, independently of object member order. Different
     * payloads remain distinct, including different proposals of the same transaction. Duplicates
     * older than the window or evicted by capacity may be forwarded again.
     */
    struct Settings {
        Clock::duration window = std::chrono::seconds{30};
        std::size_t maxMessages = 10'000;
        std::size_t maxBytes = 16 * 1024 * 1024;  // Serialized payload bytes, excluding overhead.
    };

    /**
     * @brief Construct a queue with the default cache limits.
     * @param context The context shared by the subscription sources
     * @param subscriptions The destination for forwarded stream events
     */
    SubscriptionMessageQueue(
        boost::asio::io_context& context,
        std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions
    );

    /**
     * @brief Construct a queue with custom cache limits and clock.
     * @param context The context shared by the subscription sources
     * @param subscriptions The destination for forwarded stream events
     * @param settings Cache retention and capacity limits; zero capacity disables caching
     * @param now The monotonic clock used to expire entries
     */
    SubscriptionMessageQueue(
        boost::asio::io_context& context,
        std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
        Settings settings,
        Now now = Clock::now
    );

    /**
     * @brief Queue an event and wait until it is forwarded or recognized as a duplicate.
     * @note Only proposed transactions, validations and manifests are forwarded. Oversized events
     * are forwarded without caching. Forwarding exceptions propagate to the producer.
     * @param message The incoming parsed event
     * @param yield The producer's coroutine context
     */
    void
    push(boost::json::object message, boost::asio::yield_context yield);
};

}  // namespace etl::impl
