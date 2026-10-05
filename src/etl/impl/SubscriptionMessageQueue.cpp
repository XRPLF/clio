#include "etl/impl/SubscriptionMessageQueue.hpp"

#include "feed/SubscriptionManagerInterface.hpp"

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>

#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>

namespace etl::impl {

struct SubscriptionMessageQueue::State {
    boost::asio::strand<boost::asio::io_context::executor_type> strand;
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions;
    Settings settings;
    Now now;

    struct Entry {
        std::reference_wrapper<boost::json::object const> message;
        Clock::time_point received;
        std::size_t bytes;
    };

    // References to unordered_set elements survive rehashes. Entries are removed from the FIFO
    // together with their set element, so no reference survives eviction.
    std::unordered_set<boost::json::object> seen;
    std::deque<Entry> recent;
    std::size_t bytes = 0;

    State(
        boost::asio::io_context& context,
        std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
        Settings settings,
        Now now
    )
        : strand(boost::asio::make_strand(context))
        , subscriptions(std::move(subscriptions))
        , settings(settings)
        , now(std::move(now))
    {
    }

    void
    evictOldest()
    {
        bytes -= recent.front().bytes;
        seen.erase(recent.front().message.get());
        recent.pop_front();
    }

    void
    process(boost::json::object const& message)
    {
        auto const proposed = message.contains("transaction") && !message.contains("meta");
        auto const* type = message.if_contains("type");
        auto const validation = type != nullptr && *type == "validationReceived";
        auto const manifest = type != nullptr && *type == "manifestReceived";
        if (!proposed && !validation && !manifest)
            return;

        auto const received = now();
        while (!recent.empty() && received - recent.front().received >= settings.window)
            evictOldest();
        if (seen.contains(message))
            return;

        if (proposed) {
            subscriptions->forwardProposedTransaction(message);
        } else if (validation) {
            subscriptions->forwardValidation(message);
        } else {
            subscriptions->forwardManifest(message);
        }

        // Cache only after forwarding succeeds. A failure must not suppress a subsequent copy.
        if (settings.maxMessages == 0 || settings.window <= Clock::duration::zero())
            return;
        auto const messageBytes = boost::json::serialize(message).size();
        if (messageBytes > settings.maxBytes)
            return;
        while (!recent.empty() &&
               (recent.size() >= settings.maxMessages || bytes > settings.maxBytes - messageBytes))
            evictOldest();

        auto const [it, inserted] = seen.insert(message);
        if (inserted) {
            try {
                recent.push_back(
                    {.message = std::cref(*it), .received = received, .bytes = messageBytes}
                );
            } catch (...) {
                seen.erase(it);
                throw;
            }
            bytes += messageBytes;
        }
    }
};

SubscriptionMessageQueue::SubscriptionMessageQueue(
    boost::asio::io_context& context,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions
)
    : SubscriptionMessageQueue(context, std::move(subscriptions), Settings{})
{
}

SubscriptionMessageQueue::SubscriptionMessageQueue(
    boost::asio::io_context& context,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    Settings settings,
    Now now
)
    : state_(std::make_shared<State>(context, std::move(subscriptions), settings, std::move(now)))
{
}

void
SubscriptionMessageQueue::push(boost::json::object message, boost::asio::yield_context yield)
{
    boost::asio::async_initiate<boost::asio::yield_context, void(std::exception_ptr)>(
        [state = state_, message = std::move(message)](auto completion) mutable {
            boost::asio::post(
                state->strand,
                [state,
                 message = std::move(message),
                 completion = std::move(completion)]() mutable {
                    std::exception_ptr error;
                    try {
                        state->process(message);
                    } catch (...) {
                        error = std::current_exception();
                    }
                    // Resume on the source's strand, after leaving the exception handler.
                    auto const executor = boost::asio::get_associated_executor(completion);
                    boost::asio::post(
                        executor, [completion = std::move(completion), error]() mutable {
                            std::move(completion)(error);
                        }
                    );
                }
            );
        },
        yield
    );
}

}  // namespace etl::impl
