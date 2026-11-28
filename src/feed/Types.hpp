#pragma once

#include "web/SubscriptionContextInterface.hpp"

#include <memory>

namespace feed {

/**
 * @brief A subscriber to the feeds, represented by its subscription context.
 */
using Subscriber = web::SubscriptionContextInterface;

/**
 * @brief Non-owning pointer to a subscriber.
 */
using SubscriberPtr = Subscriber*;

/**
 * @brief Shared ownership pointer to a subscriber.
 */
using SubscriberSharedPtr = std::shared_ptr<Subscriber>;

}  // namespace feed
