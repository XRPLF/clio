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

#include "feed/FeedTestUtil.hpp"
#include "feed/impl/SingleFeedBase.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockWsBase.hpp"
#include "util/SyncExecutionContextFixture.hpp"
#include "util/prometheus/Gauge.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

constexpr static auto FEED = R"({"test":"test"})";

using namespace feed::impl;
using namespace util::prometheus;

struct FeedBaseMockPrometheusTest : WithMockPrometheus, SyncExecutionCtxFixture {
protected:
    std::shared_ptr<web::ConnectionBase> sessionPtr;
    using SingleFeedBaseTest = SingleFeedBase<Ctx>;
    std::shared_ptr<SingleFeedBaseTest> testFeedPtr;
    MockSession* mockSessionPtr = nullptr;

    void
    SetUp() override
    {
        testFeedPtr = std::make_shared<SingleFeedBaseTest>(ctx, "testFeed");
        sessionPtr = std::make_shared<MockSession>();
        mockSessionPtr = dynamic_cast<MockSession*>(sessionPtr.get());
    }
    void
    TearDown() override
    {
        sessionPtr.reset();
        testFeedPtr.reset();
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

template <typename ExecutionContext>
class NamedSingleFeedTest : public SingleFeedBase<ExecutionContext> {
public:
    NamedSingleFeedTest(ExecutionContext& ctx) : SingleFeedBase<ExecutionContext>(ctx, "forTest")
    {
    }
};

using SingleFeedBaseTest = FeedBaseTest<NamedSingleFeedTest>;

TEST_F(SingleFeedBaseTest, Test)
{
    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(FEED))).Times(1);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    testFeedPtr->pub(FEED);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    testFeedPtr->pub(FEED);
}

TEST_F(SingleFeedBaseTest, TestAutoDisconnect)
{
    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(FEED))).Times(1);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    testFeedPtr->pub(FEED);

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
