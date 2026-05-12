#pragma once

#include "util/Taggable.hpp"

#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace web {

/**
 * @brief An interface to provide connection functionality for subscriptions.
 * @note Since subscription is only allowed for websocket connection, this interface is used only
 * for websocket connections.
 */
class SubscriptionContextInterface : public util::Taggable {
public:
    using util::Taggable::Taggable;

    /**
     * @brief Send message to the client
     *
     * @param message The message to send.
     */
    virtual void
    send(std::shared_ptr<std::string> message) = 0;

    /**
     * @brief Alias for on disconnect slot.
     */
    using OnDisconnectSlot = std::function<void(SubscriptionContextInterface*)>;

    /**
     * @brief Connect a slot to onDisconnect connection signal.
     *
     * @param slot The slot to connect.
     */
    virtual void
    onDisconnect(OnDisconnectSlot const& slot) = 0;

    /**
     * @brief Set the API subversion.
     * @param value The value to set.
     */
    virtual void
    setApiSubversion(uint32_t value) = 0;

    /**
     * @brief Get the API subversion.
     *
     * @return The API subversion.
     */
    [[nodiscard]] virtual uint32_t
    apiSubversion() const = 0;
};

/**
 * @brief An alias for shared pointer to a SubscriptionContextInterface.
 */
using SubscriptionContextPtr = std::shared_ptr<SubscriptionContextInterface>;

}  // namespace web
