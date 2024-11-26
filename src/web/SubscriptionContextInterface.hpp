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
 * @note Since subscription is only allowed for websocket connection, this interface is used only for websocket
 * connections.
 */
class SubscriptionContextInterface : public util::Taggable {
public:
    /**
     * @brief Reusing Taggable constructor
     */
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
    virtual uint32_t
    apiSubversion() const = 0;
};

/**
 * @brief An alias for shared pointer to a SubscriptionContextInterface.
 */
using SubscriptionContextPtr = std::shared_ptr<SubscriptionContextInterface>;

}  // namespace web
