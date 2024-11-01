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

#include "util/LoggerFixtures.hpp"
#include "util/Taggable.hpp"
#include "util/config/Config.hpp"
#include "web/SubscriptionContext.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/interface/ConnectionBaseMock.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace web;

struct SubscriptionContextTests : NoLoggerFixture {
    util::TagDecoratorFactory tagFactory_{util::Config{}};
    ConnectionBaseStrictMockPtr connection_ =
        std::make_shared<testing::StrictMock<ConnectionBaseMock>>(tagFactory_, "some ip");

    SubscriptionContext subscriptionContext_{tagFactory_, connection_};
    testing::StrictMock<testing::MockFunction<void(SubscriptionContextInterface*)>> callbackMock_;
};

TEST_F(SubscriptionContextTests, send)
{
    auto message = std::make_shared<std::string>("message");
    EXPECT_CALL(*connection_, send(message));
    subscriptionContext_.send(message);
}

TEST_F(SubscriptionContextTests, sendConnectionExpired)
{
    auto message = std::make_shared<std::string>("message");
    connection_.reset();
    subscriptionContext_.send(message);
}

TEST_F(SubscriptionContextTests, onDisconnect)
{
    auto localContext = std::make_unique<SubscriptionContext>(tagFactory_, connection_);
    localContext->onDisconnect(callbackMock_.AsStdFunction());

    EXPECT_CALL(callbackMock_, Call(localContext.get()));
    localContext.reset();
}

TEST_F(SubscriptionContextTests, setApiSubversion)
{
    EXPECT_EQ(subscriptionContext_.apiSubversion(), 0);
    subscriptionContext_.setApiSubversion(42);
    EXPECT_EQ(subscriptionContext_.apiSubversion(), 42);
}
