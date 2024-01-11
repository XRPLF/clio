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

#include "feed/impl/TrackableSignal.h"
#include "feed/impl/TrackableSignalMap.h"
#include "util/MockWsBase.h"
#include "util/Taggable.h"
#include "util/config/Config.h"
#include "web/interface/ConnectionBase.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace testing;

struct FeedTrackableSignalTests : Test {
protected:
    std::shared_ptr<web::ConnectionBase> sessionPtr;

    void
    SetUp() override
    {
        sessionPtr = std::make_shared<MockSession>();
    }

    void
    TearDown() override
    {
    }
};

TEST_F(FeedTrackableSignalTests, Connect)
{
    feed::impl::TrackableSignal<web::ConnectionBase, std::string> signal;
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
    feed::impl::TrackableSignal<web::ConnectionBase, std::string> signal;
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
    feed::impl::TrackableSignalMap<std::string, web::ConnectionBase, std::string> signalMap;
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
    feed::impl::TrackableSignalMap<std::string, web::ConnectionBase, std::string> signalMap;
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
