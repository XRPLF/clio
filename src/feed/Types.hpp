#pragma once

#include "web/SubscriptionContextInterface.hpp"

#include <memory>

namespace feed {

using Subscriber = web::SubscriptionContextInterface;
using SubscriberPtr = Subscriber*;
using SubscriberSharedPtr = std::shared_ptr<Subscriber>;

}  // namespace feed
