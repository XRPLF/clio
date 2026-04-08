#pragma once

#include "util/Taggable.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/interface/Concepts.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/signals2/variadic_signal.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace web {

/**
 * @brief A context of a WsBase connection for subscriptions.
 */
class SubscriptionContext : public SubscriptionContextInterface {
    std::weak_ptr<ConnectionBase> connection_;
    boost::signals2::signal<void(SubscriptionContextInterface*)> onDisconnect_;
    /**
     * @brief The API version of the web stream client.
     * This is used to track the api version of this connection, which mainly is used by
     * subscription. It is different from the api version in Context, which is only used for the
     * current request.
     */
    std::atomic_uint32_t apiSubVersion_ = 0;

public:
    /**
     * @brief Construct a new Subscription Context object
     *
     * @param factory The tag decorator factory to use to init taggable.
     * @param connection The connection for which the context is created.
     */
    SubscriptionContext(
        util::TagDecoratorFactory const& factory,
        std::shared_ptr<ConnectionBase> connection
    );

    /**
     * @brief Destroy the Subscription Context object
     */
    ~SubscriptionContext() override;

    /**
     * @brief Send message to the client
     * @note This method will not do anything if the related connection got disconnected.
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
    uint32_t
    apiSubversion() const override;
};

}  // namespace web
