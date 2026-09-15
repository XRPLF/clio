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

namespace {

struct DestructorDisconnectingSubscriber;
using ReentrantSignalMap =
    feed::impl::TrackableSignalMap<std::string, DestructorDisconnectingSubscriber, int>;

struct DestructorDisconnectingSubscriber {
    ReentrantSignalMap* signalMap = nullptr;
    bool* disconnectFinished = nullptr;

    ~DestructorDisconnectingSubscriber()
    {
        signalMap->disconnect(this, "key");
        *disconnectFinished = true;
    }
};

}  // namespace

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

TEST_F(FeedTrackableSignalTests, MapDisconnectFromTrackableDestructorWhileEmitting)
{
    ReentrantSignalMap signalMap;
    bool disconnectFinished = false;

    auto subscriber = std::make_shared<DestructorDisconnectingSubscriber>();
    subscriber->signalMap = &signalMap;
    subscriber->disconnectFinished = &disconnectFinished;

    bool slotCalled = false;
    ASSERT_TRUE(signalMap.connectTrackableSlot(subscriber, "key", [&](int) {
        slotCalled = true;
        // The session died while the slot is running; the signal now holds the last reference.
        subscriber.reset();
    }));

    signalMap.emit("key", 42);

    EXPECT_TRUE(slotCalled);
    EXPECT_TRUE(disconnectFinished);

    // The entry was removed by the destructor's disconnect, so nothing is left to notify.
    slotCalled = false;
    signalMap.emit("key", 42);
    EXPECT_FALSE(slotCalled);
}
