#include "feed/FeedTestUtil.hpp"
#include "feed/impl/ForwardFeed.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using namespace feed::impl;
namespace json = boost::json;
using namespace util::prometheus;

namespace {

constexpr auto kFEED = R"JSON({"test": "test"})JSON";

}  // namespace

class NamedForwardFeedTest : public ForwardFeed {
public:
    NamedForwardFeedTest(util::async::AnyExecutionContext& executionCtx)
        : ForwardFeed(executionCtx, "test")
    {
    }
};

using FeedForwardTest = FeedBaseTest<NamedForwardFeedTest>;

TEST_F(FeedForwardTest, Pub)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);

    auto const json = json::parse(kFEED).as_object();
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kFEED))).Times(1);
    testFeedPtr->pub(json);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    testFeedPtr->pub(json);
}

TEST_F(FeedForwardTest, AutoDisconnect)
{
    web::SubscriptionContextInterface::OnDisconnectSlot slot;
    EXPECT_CALL(*mockSessionPtr, onDisconnect).WillOnce(testing::SaveArg<0>(&slot));
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);

    auto const json = json::parse(kFEED).as_object();
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kFEED)));
    testFeedPtr->pub(json);

    slot(sessionPtr.get());
    sessionPtr.reset();
    EXPECT_EQ(testFeedPtr->count(), 0);

    testFeedPtr->pub(json);
}
