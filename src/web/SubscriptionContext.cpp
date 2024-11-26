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
