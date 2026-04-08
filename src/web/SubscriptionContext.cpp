#include "web/SubscriptionContext.hpp"

#include "util/Taggable.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace web {

SubscriptionContext::SubscriptionContext(
    util::TagDecoratorFactory const& factory,
    std::shared_ptr<ConnectionBase> connection
)
    : SubscriptionContextInterface{factory}, connection_{connection}
{
}

SubscriptionContext::~SubscriptionContext()
{
    onDisconnect_(this);
}

void
SubscriptionContext::send(std::shared_ptr<std::string> message)
{
    if (auto connection = connection_.lock(); connection != nullptr)
        connection->send(std::move(message));
}

void
SubscriptionContext::onDisconnect(OnDisconnectSlot const& slot)
{
    onDisconnect_.connect(slot);
}

void
SubscriptionContext::setApiSubversion(uint32_t value)
{
    apiSubVersion_ = value;
}

uint32_t
SubscriptionContext::apiSubversion() const
{
    return apiSubVersion_;
}

}  // namespace web
