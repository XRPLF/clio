#include "web/ng/SubscriptionContext.hpp"

#include "util/Assert.hpp"
#include "util/Taggable.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/asio/spawn.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace web::ng {

SubscriptionContext::SubscriptionContext(
    util::TagDecoratorFactory const& factory,
    impl::WsConnectionBase& connection,
    std::optional<size_t> maxSendQueueSize,
    boost::asio::yield_context yield,
    ErrorHandler errorHandler
)
    : web::SubscriptionContextInterface(factory)
    , connection_(connection)
    , maxSendQueueSize_(maxSendQueueSize)
    , tasksGroup_(yield)
    , yield_(yield)
    , errorHandler_(std::move(errorHandler))
{
}

SubscriptionContext::~SubscriptionContext()
{
    ASSERT(disconnected_, "SubscriptionContext must be disconnected before destroying");
}

void
SubscriptionContext::send(std::shared_ptr<std::string> message)
{
    if (disconnected_ or gotError_)
        return;

    if (maxSendQueueSize_.has_value() and tasksGroup_.size() >= *maxSendQueueSize_) {
        tasksGroup_.spawn(yield_, [this](boost::asio::yield_context innerYield) {
            connection_.get().close(innerYield);
        });
        gotError_ = true;
        return;
    }

    tasksGroup_.spawn(
        yield_,
        [this, message = std::move(message)](boost::asio::yield_context innerYield) mutable {
            auto const expectedSuccess =
                connection_.get().sendShared(std::move(message), innerYield);
            if (not expectedSuccess.has_value() and
                errorHandler_(expectedSuccess.error(), connection_)) {
                connection_.get().close(innerYield);
                gotError_ = true;
            }
        }
    );
}

void
SubscriptionContext::onDisconnect(OnDisconnectSlot const& slot)
{
    onDisconnect_.connect(slot);
}

void
SubscriptionContext::setApiSubversion(uint32_t value)
{
    apiSubversion_ = value;
}

uint32_t
SubscriptionContext::apiSubversion() const
{
    return apiSubversion_;
}

void
SubscriptionContext::disconnect(boost::asio::yield_context yield)
{
    disconnected_ = true;
    onDisconnect_(this);
    tasksGroup_.asyncWait(yield);
}

}  // namespace web::ng
