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
#include "feed/impl/ForwardFeed.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace feed::impl;
namespace json = boost::json;
using namespace util::prometheus;

constexpr static auto FEED = R"({"test":"test"})";

class NamedForwardFeedTest : public ForwardFeed {
public:
    NamedForwardFeedTest(boost::asio::io_context& ioContext) : ForwardFeed(ioContext, "test")
    {
    }
};

using FeedForwardTest = FeedBaseTest<NamedForwardFeedTest>;

TEST_F(FeedForwardTest, Pub)
{
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    auto const json = json::parse(FEED).as_object();
    testFeedPtr->pub(json);
    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(FEED))).Times(1);
    ctx.run();

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    testFeedPtr->pub(json);
    ctx.restart();
    ctx.run();
}

TEST_F(FeedForwardTest, AutoDisconnect)
{
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    auto const json = json::parse(FEED).as_object();
    testFeedPtr->pub(json);
    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(FEED))).Times(1);
    ctx.run();
    sessionPtr.reset();
    EXPECT_EQ(testFeedPtr->count(), 0);
    testFeedPtr->pub(json);
}
