#include "feed/impl/TrackableSignal.hpp"
#include "feed/impl/TrackableSignalMap.hpp"
#include "util/MockWsBase.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace testing;

struct FeedTrackableSignalTests : Test {
    web::SubscriptionContextPtr sessionPtr = std::make_shared<MockSession>();
};

TEST_F(FeedTrackableSignalTests, Connect)
{
    feed::impl::TrackableSignal<web::SubscriptionContextInterface, std::string> signal;
    std::string testString;
    auto const slot = [&](std::string const& s) { testString += s; };
    EXPECT_TRUE(signal.connectTrackableSlot(sessionPtr, slot));
    EXPECT_FALSE(signal.connectTrackableSlot(sessionPtr, slot));

    EXPECT_EQ(signal.count(), 1);

    signal.emit("test");
    EXPECT_EQ(testString, "test");

    EXPECT_TRUE(signal.disconnect(sessionPtr.get()));
    EXPECT_EQ(signal.count(), 0);
    EXPECT_FALSE(signal.disconnect(sessionPtr.get()));

    testString.clear();
    signal.emit("test2");
    EXPECT_TRUE(testString.empty());
}

TEST_F(FeedTrackableSignalTests, AutoDisconnect)
{
    feed::impl::TrackableSignal<web::SubscriptionContextInterface, std::string> signal;
    std::string testString;
    auto const slot = [&](std::string const& s) { testString += s; };
    EXPECT_TRUE(signal.connectTrackableSlot(sessionPtr, slot));
    EXPECT_FALSE(signal.connectTrackableSlot(sessionPtr, slot));

    EXPECT_EQ(signal.count(), 1);

    signal.emit("test");
    EXPECT_EQ(testString, "test");

    sessionPtr.reset();
    // track object is destroyed, but the connection is still there
    EXPECT_EQ(signal.count(), 1);

    testString.clear();
    signal.emit("test2");
    EXPECT_TRUE(testString.empty());
}

TEST_F(FeedTrackableSignalTests, MapConnect)
{
    feed::impl::TrackableSignalMap<std::string, web::SubscriptionContextInterface, std::string>
        signalMap;
    std::string testString;
    auto const slot = [&](std::string const& s) { testString += s; };
    EXPECT_TRUE(signalMap.connectTrackableSlot(sessionPtr, "test", slot));
    EXPECT_TRUE(signalMap.connectTrackableSlot(sessionPtr, "test1", slot));
    EXPECT_FALSE(signalMap.connectTrackableSlot(sessionPtr, "test", slot));

    signalMap.emit("test", "test");
    signalMap.emit("test2", "test2");
    EXPECT_EQ(testString, "test");

    EXPECT_TRUE(signalMap.disconnect(sessionPtr.get(), "test"));
    EXPECT_FALSE(signalMap.disconnect(sessionPtr.get(), "test"));

    testString.clear();
    signalMap.emit("test", "test2");
    EXPECT_TRUE(testString.empty());

    signalMap.emit("test1", "test1");
    EXPECT_EQ(testString, "test1");
}

TEST_F(FeedTrackableSignalTests, MapAutoDisconnect)
{
    feed::impl::TrackableSignalMap<std::string, web::SubscriptionContextInterface, std::string>
        signalMap;
    std::string testString;
    auto const slot = [&](std::string const& s) { testString += s; };
    EXPECT_TRUE(signalMap.connectTrackableSlot(sessionPtr, "test", slot));
    EXPECT_TRUE(signalMap.connectTrackableSlot(sessionPtr, "test1", slot));
    EXPECT_FALSE(signalMap.connectTrackableSlot(sessionPtr, "test", slot));

    signalMap.emit("test", "test");
    signalMap.emit("test2", "test2");
    EXPECT_EQ(testString, "test");

    // kill trackable
    sessionPtr.reset();

    testString.clear();
    signalMap.emit("test", "test");
    EXPECT_TRUE(testString.empty());

    signalMap.emit("test1", "test1");
    EXPECT_TRUE(testString.empty());
}
