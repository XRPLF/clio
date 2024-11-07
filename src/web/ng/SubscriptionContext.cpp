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

#include "web/ng/SubscriptionContext.hpp"

#include "util/Taggable.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
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

void
SubscriptionContext::send(std::shared_ptr<std::string> message)
{
    if (disconnected_)
        return;

    if (maxSendQueueSize_.has_value() and tasksGroup_.size() >= *maxSendQueueSize_) {
        tasksGroup_.spawn(yield_, [this](boost::asio::yield_context innerYield) {
            connection_.get().close(innerYield);
        });
        disconnected_ = true;
        return;
    }

    tasksGroup_.spawn(yield_, [this, message = std::move(message)](boost::asio::yield_context innerYield) {
        auto const maybeError = connection_.get().sendBuffer(boost::asio::buffer(*message), innerYield);
        if (maybeError.has_value() and errorHandler_(*maybeError, connection_))
            connection_.get().close(innerYield);
    });
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
    onDisconnect_(this);
    disconnected_ = true;
    tasksGroup_.asyncWait(yield);
}

}  // namespace web::ng
