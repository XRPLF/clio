#pragma once

#include "util/CoroutineGroup.hpp"
#include "util/Taggable.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/impl/WsConnection.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace web::ng {

/**
 * @brief Implementation of SubscriptionContextInterface.
 * @note This class is designed to be used with SubscriptionManager. The class is safe to use from
 * multiple threads. The method disconnect() must be called before the object is destroyed.
 */
class SubscriptionContext : public web::SubscriptionContextInterface {
public:
    /**
     * @brief Error handler definition. Error handler returns true if connection should be closed
     * false otherwise.
     */
    using ErrorHandler = std::function<bool(Error const&, Connection const&)>;

private:
    std::reference_wrapper<impl::WsConnectionBase> connection_;
    std::optional<size_t> maxSendQueueSize_;
    util::CoroutineGroup tasksGroup_;
    boost::asio::yield_context yield_;
    ErrorHandler errorHandler_;

    boost::signals2::signal<void(SubscriptionContextInterface*)> onDisconnect_;
    std::atomic_bool disconnected_{false};
    std::atomic_bool gotError_{false};

    /**
     * @brief The API version of the web stream client.
     * This is used to track the api version of this connection, which mainly is used by
     * subscription. It is different from the api version in Context, which is only used for the
     * current request.
     */
    std::atomic_uint32_t apiSubversion_ = 0u;

public:
    /**
     * @brief Construct a new Subscription Context object
     *
     * @param factory The tag decorator factory to use to init taggable.
     * @param connection The connection for which the context is created.
     * @param maxSendQueueSize The maximum size of the send queue. If the queue is full, the
     * connection will be closed.
     * @param yield The yield context to spawn sending coroutines.
     * @param errorHandler The error handler.
     */
    SubscriptionContext(
        util::TagDecoratorFactory const& factory,
        impl::WsConnectionBase& connection,
        std::optional<size_t> maxSendQueueSize,
        boost::asio::yield_context yield,
        ErrorHandler errorHandler
    );

    ~SubscriptionContext() override;

    /**
     * @brief Send message to the client
     * @note This method does nothing after disconnected() was called.
     *
     * @param message The message to send.
     */
    void
    send(std::shared_ptr<std::string> message) override;

    /**
     * @brief Connect a slot to onDisconnect connection signal.
     *
     * @param slot The slot to connect.
     */
    void
    onDisconnect(OnDisconnectSlot const& slot) override;

    /**
     * @brief Set the API subversion.
     * @param value The value to set.
     */
    void
    setApiSubversion(uint32_t value) override;

    /**
     * @brief Get the API subversion.
     *
     * @return The API subversion.
     */
    [[nodiscard]] uint32_t
    apiSubversion() const override;

    /**
     * @brief Notify the context that related connection is disconnected and wait for all the task
     * to complete.
     * @note This method must be called before the object is destroyed.
     *
     * @param yield The yield context to wait for all the tasks to complete.
     */
    void
    disconnect(boost::asio::yield_context yield);
};

}  // namespace web::ng
