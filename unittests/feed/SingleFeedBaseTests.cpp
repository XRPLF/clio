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

#include "feed/FeedBaseTest.h"
#include "feed/impl/SingleFeedBase.h"
#include "util/Fixtures.h"
#include "util/MockPrometheus.h"
#include "util/MockWsBase.h"
#include "util/Taggable.h"
#include "util/config/Config.h"
#include "util/prometheus/Gauge.h"
#include "web/interface/ConnectionBase.h"

#include <boost/asio/io_context.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

constexpr static auto FEED = R"({"test":"test"})";

using namespace feed::impl;
using namespace util::prometheus;

struct FeedBaseMockPrometheusTest : WithMockPrometheus, SyncAsioContextTest {
protected:
    util::TagDecoratorFactory tagDecoratorFactory{util::Config{}};
    std::shared_ptr<web::ConnectionBase> sessionPtr;
    std::shared_ptr<SingleFeedBase> testFeedPtr;

    void
    SetUp() override
    {
        WithMockPrometheus::SetUp();
        SyncAsioContextTest::SetUp();
        testFeedPtr = std::make_shared<SingleFeedBase>(ctx, "testFeed");
        sessionPtr = std::make_shared<MockSession>(tagDecoratorFactory);
    }
    void
    TearDown() override
    {
        sessionPtr.reset();
        testFeedPtr.reset();
        SyncAsioContextTest::TearDown();
        WithMockPrometheus::TearDown();
    }
};

TEST_F(FeedBaseMockPrometheusTest, subUnsub)
{
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"testFeed\"}");
    EXPECT_CALL(counter, add(1));
    EXPECT_CALL(counter, add(-1));

    testFeedPtr->sub(sessionPtr);
    testFeedPtr->unsub(sessionPtr);
}

TEST_F(FeedBaseMockPrometheusTest, AutoUnsub)
{
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"testFeed\"}");
    EXPECT_CALL(counter, add(1));
    EXPECT_CALL(counter, add(-1));

    testFeedPtr->sub(sessionPtr);
    sessionPtr.reset();
}

class NamedSingleFeedTest : public SingleFeedBase {
public:
    NamedSingleFeedTest(boost::asio::io_context& ioContext) : SingleFeedBase(ioContext, "forTest")
    {
    }
};

using SingleFeedBaseTest = FeedBaseTest<NamedSingleFeedTest>;

TEST_F(SingleFeedBaseTest, Test)
{
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    testFeedPtr->pub(FEED);
    ctx.run();

    EXPECT_EQ(receivedFeedMessage(), FEED);
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    cleanReceivedFeed();
    testFeedPtr->pub(FEED);
    ctx.restart();
    ctx.run();
    EXPECT_TRUE(receivedFeedMessage().empty());
}

TEST_F(SingleFeedBaseTest, TestAutoDisconnect)
{
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    testFeedPtr->pub(FEED);
    ctx.run();

    EXPECT_EQ(receivedFeedMessage(), FEED);
    sessionPtr.reset();
    EXPECT_EQ(testFeedPtr->count(), 0);
}

TEST_F(SingleFeedBaseTest, RepeatSub)
{
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
}
