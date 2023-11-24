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

#include <feed/SubscriptionManager.h>

#include <util/Fixtures.h>
#include <util/MockPrometheus.h>
#include <util/MockWsBase.h>

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>

using namespace feed;
using namespace util::prometheus;

// io_context
struct SubscriptionTestBase {
    util::Config cfg;
    util::TagDecoratorFactory tagDecoratorFactory{cfg};
};

struct SubscriptionTest : WithPrometheus, SyncAsioContextTest, SubscriptionTestBase {
    Subscription sub{ctx, "test"};
};

// subscribe/unsubscribe the same session would not change the count
TEST_F(SubscriptionTest, SubscriptionCount)
{
    std::shared_ptr<web::ConnectionBase> const session1 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<web::ConnectionBase> const session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    sub.subscribe(session1);
    sub.subscribe(session2);
    ctx.run();
    EXPECT_EQ(sub.count(), 2);
    sub.subscribe(session1);
    ctx.restart();
    ctx.run();
    EXPECT_EQ(sub.count(), 2);
    EXPECT_TRUE(sub.hasSession(session1));
    EXPECT_TRUE(sub.hasSession(session2));
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
    EXPECT_FALSE(sub.hasSession(session1));
    EXPECT_FALSE(sub.hasSession(session2));
}

// send interface will be called when publish called
TEST_F(SubscriptionTest, SubscriptionPublish)
{
    std::shared_ptr<web::ConnectionBase> const session1 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<web::ConnectionBase> const session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    sub.subscribe(session1);
    sub.subscribe(session2);
    ctx.run();
    EXPECT_EQ(sub.count(), 2);
    sub.publish(std::make_shared<std::string>("message"));
    ctx.restart();
    ctx.run();
    MockSession* p1 = dynamic_cast<MockSession*>(session1.get());
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->message, "message");
    MockSession* p2 = dynamic_cast<MockSession*>(session2.get());
    ASSERT_NE(p2, nullptr);
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
    std::shared_ptr<web::ConnectionBase> const session1(new MockDeadSession(tagDecoratorFactory));
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

struct SubscriptionMockPrometheusTest : WithMockPrometheus, SubscriptionTestBase, SyncAsioContextTest {
    Subscription sub{ctx, "test"};
    std::shared_ptr<web::ConnectionBase> const session = std::make_shared<MockSession>(tagDecoratorFactory);
};

TEST_F(SubscriptionMockPrometheusTest, subscribe)
{
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"test\"}");
    EXPECT_CALL(counter, add(1));
    sub.subscribe(session);
    ctx.run();
}

TEST_F(SubscriptionMockPrometheusTest, unsubscribe)
{
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"test\"}");
    EXPECT_CALL(counter, add(1));
    sub.subscribe(session);
    ctx.run();
    EXPECT_CALL(counter, add(-1));
    sub.unsubscribe(session);
    ctx.restart();
    ctx.run();
}

TEST_F(SubscriptionMockPrometheusTest, publish)
{
    auto deadSession = std::make_shared<MockDeadSession>(tagDecoratorFactory);
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"test\"}");
    EXPECT_CALL(counter, add(1));
    sub.subscribe(deadSession);
    ctx.run();
    EXPECT_CALL(counter, add(-1));
    sub.publish(std::make_shared<std::string>("message"));
    sub.publish(std::make_shared<std::string>("message"));  // Dead session is detected only after failed send
    ctx.restart();
    ctx.run();
}

TEST_F(SubscriptionMockPrometheusTest, count)
{
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"test\"}");
    EXPECT_CALL(counter, value());
    sub.count();
}

struct SubscriptionMapTest : SubscriptionTest {
    SubscriptionMap<std::string> subMap{ctx, "test"};
};

TEST_F(SubscriptionMapTest, SubscriptionMapCount)
{
    std::shared_ptr<web::ConnectionBase> const session1 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<web::ConnectionBase> const session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<web::ConnectionBase> const session3 = std::make_shared<MockSession>(tagDecoratorFactory);
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
    EXPECT_TRUE(subMap.hasSession(session1, "topic1"));
    EXPECT_TRUE(subMap.hasSession(session2, "topic1"));
    EXPECT_TRUE(subMap.hasSession(session3, "topic2"));
    subMap.unsubscribe(session1, "topic1");
    ctx.restart();
    ctx.run();
    subMap.unsubscribe(session1, "topic1");
    subMap.unsubscribe(session2, "topic1");
    subMap.unsubscribe(session3, "topic2");
    ctx.restart();
    ctx.run();
    EXPECT_FALSE(subMap.hasSession(session1, "topic1"));
    EXPECT_FALSE(subMap.hasSession(session2, "topic1"));
    EXPECT_FALSE(subMap.hasSession(session3, "topic2"));
    EXPECT_EQ(subMap.count(), 0);
    subMap.unsubscribe(session3, "topic2");
    subMap.unsubscribe(session3, "no exist");
    ctx.restart();
    ctx.run();
    EXPECT_EQ(subMap.count(), 0);
}

TEST_F(SubscriptionMapTest, SubscriptionMapPublish)
{
    std::shared_ptr<web::ConnectionBase> const session1 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::shared_ptr<web::ConnectionBase> const session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::string const topic1 = "topic1";
    std::string const topic2 = "topic2";
    std::string const topic1Message = "topic1Message";
    std::string const topic2Message = "topic2Message";
    subMap.subscribe(session1, topic1);
    subMap.subscribe(session2, topic2);
    ctx.run();
    EXPECT_EQ(subMap.count(), 2);
    auto message1 = std::make_shared<std::string>(topic1Message.data());
    subMap.publish(message1, topic1);                                             // lvalue
    subMap.publish(std::make_shared<std::string>(topic2Message.data()), topic2);  // rvalue
    ctx.restart();
    ctx.run();
    MockSession* p1 = dynamic_cast<MockSession*>(session1.get());
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->message, topic1Message);
    MockSession* p2 = dynamic_cast<MockSession*>(session2.get());
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->message, topic2Message);
}

TEST_F(SubscriptionMapTest, SubscriptionMapDeadRemoveSubscriber)
{
    std::shared_ptr<web::ConnectionBase> const session1(new MockDeadSession(tagDecoratorFactory));
    std::shared_ptr<web::ConnectionBase> const session2 = std::make_shared<MockSession>(tagDecoratorFactory);
    std::string const topic1 = "topic1";
    std::string const topic2 = "topic2";
    std::string const topic1Message = "topic1Message";
    std::string const topic2Message = "topic2Message";
    subMap.subscribe(session1, topic1);
    subMap.subscribe(session2, topic2);
    ctx.run();
    EXPECT_EQ(subMap.count(), 2);
    auto message1 = std::make_shared<std::string>(topic1Message);
    subMap.publish(message1, topic1);                                      // lvalue
    subMap.publish(std::make_shared<std::string>(topic2Message), topic2);  // rvalue
    ctx.restart();
    ctx.run();
    MockDeadSession* p1 = dynamic_cast<MockDeadSession*>(session1.get());
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(p1->dead(), true);
    MockSession* p2 = dynamic_cast<MockSession*>(session2.get());
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p2->message, topic2Message);
    subMap.publish(message1, topic1);
    ctx.restart();
    ctx.run();
    EXPECT_EQ(subMap.count(), 1);
}

struct SubscriptionMapMockPrometheusTest : SubscriptionMockPrometheusTest {
    SubscriptionMap<std::string> subMap{ctx, "test"};
    std::shared_ptr<web::ConnectionBase> const session = std::make_shared<MockSession>(tagDecoratorFactory);
};

TEST_F(SubscriptionMapMockPrometheusTest, subscribe)
{
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{collection=\"test\"}");
    EXPECT_CALL(counter, add(1));
    subMap.subscribe(session, "topic");
    ctx.run();
}

TEST_F(SubscriptionMapMockPrometheusTest, unsubscribe)
{
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{collection=\"test\"}");
    EXPECT_CALL(counter, add(1));
    subMap.subscribe(session, "topic");
    ctx.run();
    EXPECT_CALL(counter, add(-1));
    subMap.unsubscribe(session, "topic");
    ctx.restart();
    ctx.run();
}

TEST_F(SubscriptionMapMockPrometheusTest, publish)
{
    auto deadSession = std::make_shared<MockDeadSession>(tagDecoratorFactory);
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{collection=\"test\"}");
    EXPECT_CALL(counter, add(1));
    subMap.subscribe(deadSession, "topic");
    ctx.run();
    EXPECT_CALL(counter, add(-1));
    subMap.publish(std::make_shared<std::string>("message"), "topic");
    subMap.publish(
        std::make_shared<std::string>("message"), "topic"
    );  // Dead session is detected only after failed send
    ctx.restart();
    ctx.run();
}

TEST_F(SubscriptionMapMockPrometheusTest, count)
{
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{collection=\"test\"}");
    EXPECT_CALL(counter, value());
    subMap.count();
}
