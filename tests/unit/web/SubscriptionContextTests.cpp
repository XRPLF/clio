#include "util/Taggable.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "web/SubscriptionContext.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/interface/ConnectionBaseMock.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace web;
using namespace util::config;

struct SubscriptionContextTests : public virtual ::testing::Test {
protected:
    util::TagDecoratorFactory tagFactory_{ClioConfigDefinition{
        {"log.tag_style", ConfigValue{ConfigType::String}.defaultValue("uint")},
    }};
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
