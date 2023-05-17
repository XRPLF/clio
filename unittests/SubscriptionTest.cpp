//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <subscriptions/Message.h>
#include <subscriptions/SubscriptionManager.h>

#include <util/Fixtures.h>
#include <util/MockWsBase.h>

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>

namespace json = boost::json;

TEST(MessageTest, Message)
{
    auto m = Message{"test"};
    EXPECT_STREQ(m.data(), "test");
    EXPECT_EQ(m.size(), 4);
}

// io_context
class SubscriptionTest : public SyncAsioContextTest
{
protected:
    clio::Config cfg;
    util::TagDecoratorFactory tagDecoratorFactory{cfg};
};

class SubscriptionMapTest : public SubscriptionTest
{
};

// subscribe/unsubscribe the same session would not change the count
TEST_F(SubscriptionTest, SubscriptionCount)
{
    Subscription sub(ctx);
    std::shared_ptr<Server::ConnectionBase> session1 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<Server::ConnectionBase> session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    sub.subscribe(session1);
    sub.subscribe(session2);
    ctx.run();
    EXPECT_EQ(sub.count(), 2);
    sub.subscribe(session1);
    ctx.restart();
    ctx.run();
    EXPECT_EQ(sub.count(), 2);
    EXPECT_FALSE(sub.empty());
    sub.unsubscribe(session1);
    ctx.restart();
    ctx.run();
    EXPECT_EQ(sub.count(), 1);
    sub.unsubscribe(session1);
    ctx.restart();
    ctx.run();
    EXPECT_EQ(sub.count(), 1);
    sub.unsubscribe(session2);
    ctx.restart();
    ctx.run();
    EXPECT_EQ(sub.count(), 0);
    EXPECT_TRUE(sub.empty());
}

// send interface will be called when publish called
TEST_F(SubscriptionTest, SubscriptionPublish)
{
    Subscription sub(ctx);
    std::shared_ptr<Server::ConnectionBase> session1 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<Server::ConnectionBase> session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    sub.subscribe(session1);
    sub.subscribe(session2);
    ctx.run();
    EXPECT_EQ(sub.count(), 2);
    sub.publish(std::make_shared<std::string>("message"));
    ctx.restart();
    ctx.run();
    MockSession* p1 = (MockSession*)(session1.get());
    EXPECT_EQ(p1->message, "message");
    MockSession* p2 = (MockSession*)(session2.get());
    EXPECT_EQ(p2->message, "message");
    sub.unsubscribe(session1);
    ctx.restart();
    ctx.run();
    sub.publish(std::make_shared<std::string>("message2"));
    ctx.restart();
    ctx.run();
    EXPECT_EQ(p1->message, "message");
    EXPECT_EQ(p2->message, "messagemessage2");
}

// when error happen during send(), the subsciber will be removed after
TEST_F(SubscriptionTest, SubscriptionDeadRemoveSubscriber)
{
    Subscription sub(ctx);
    std::shared_ptr<Server::ConnectionBase> session1(new MockDeadSession(tagDecoratorFactory));
    sub.subscribe(session1);
    ctx.run();
    EXPECT_EQ(sub.count(), 1);
    // trigger dead
    sub.publish(std::make_shared<std::string>("message"));
    ctx.restart();
    ctx.run();
    EXPECT_EQ(session1->dead(), true);
    sub.publish(std::make_shared<std::string>("message"));
    ctx.restart();
    ctx.run();
    EXPECT_EQ(sub.count(), 0);
}

TEST_F(SubscriptionMapTest, SubscriptionMapCount)
{
    std::shared_ptr<Server::ConnectionBase> session1 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<Server::ConnectionBase> session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<Server::ConnectionBase> session3 = std::make_shared<MockSession>(tagDecoratorFactory);
    SubscriptionMap<std::string> subMap(ctx);
    subMap.subscribe(session1, "topic1");
    subMap.subscribe(session2, "topic1");
    subMap.subscribe(session3, "topic2");
    ctx.run();
    EXPECT_EQ(subMap.count(), 3);
    subMap.subscribe(session1, "topic1");
    subMap.subscribe(session2, "topic1");
    ctx.restart();
    ctx.run();
    EXPECT_EQ(subMap.count(), 3);
    subMap.unsubscribe(session1, "topic1");
    ctx.restart();
    ctx.run();
    subMap.unsubscribe(session1, "topic1");
    subMap.unsubscribe(session2, "topic1");
    subMap.unsubscribe(session3, "topic2");
    ctx.restart();
    ctx.run();
    EXPECT_EQ(subMap.count(), 0);
    subMap.unsubscribe(session3, "topic2");
    subMap.unsubscribe(session3, "no exist");
    ctx.restart();
    ctx.run();
    EXPECT_EQ(subMap.count(), 0);
}

TEST_F(SubscriptionMapTest, SubscriptionMapPublish)
{
    std::shared_ptr<Server::ConnectionBase> session1 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<Server::ConnectionBase> session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    SubscriptionMap<std::string> subMap(ctx);
    const std::string topic1 = "topic1";
    const std::string topic2 = "topic2";
    const std::string topic1Message = "topic1Message";
    const std::string topic2Message = "topic2Message";
    subMap.subscribe(session1, topic1);
    subMap.subscribe(session2, topic2);
    ctx.run();
    EXPECT_EQ(subMap.count(), 2);
    auto message1 = std::make_shared<std::string>(topic1Message.data());
    subMap.publish(message1, topic1);                                             // lvalue
    subMap.publish(std::make_shared<std::string>(topic2Message.data()), topic2);  // rvalue
    ctx.restart();
    ctx.run();
    MockSession* p1 = (MockSession*)(session1.get());
    EXPECT_EQ(p1->message, topic1Message);
    MockSession* p2 = (MockSession*)(session2.get());
    EXPECT_EQ(p2->message, topic2Message);
}

TEST_F(SubscriptionMapTest, SubscriptionMapDeadRemoveSubscriber)
{
    std::shared_ptr<Server::ConnectionBase> session1(new MockDeadSession(tagDecoratorFactory));
    std::shared_ptr<Server::ConnectionBase> session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    SubscriptionMap<std::string> subMap(ctx);
    const std::string topic1 = "topic1";
    const std::string topic2 = "topic2";
    const std::string topic1Message = "topic1Message";
    const std::string topic2Message = "topic2Message";
    subMap.subscribe(session1, topic1);
    subMap.subscribe(session2, topic2);
    ctx.run();
    EXPECT_EQ(subMap.count(), 2);
    auto message1 = std::make_shared<std::string>(topic1Message);
    subMap.publish(message1, topic1);                                      // lvalue
    subMap.publish(std::make_shared<std::string>(topic2Message), topic2);  // rvalue
    ctx.restart();
    ctx.run();
    MockDeadSession* p1 = (MockDeadSession*)(session1.get());
    EXPECT_EQ(p1->dead(), true);
    MockSession* p2 = (MockSession*)(session2.get());
    EXPECT_EQ(p2->message, topic2Message);
    subMap.publish(message1, topic1);
    ctx.restart();
    ctx.run();
    EXPECT_EQ(subMap.count(), 1);
}
