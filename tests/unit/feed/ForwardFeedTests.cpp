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

#include <memory>

using namespace feed::impl;
namespace json = boost::json;
using namespace util::prometheus;

constexpr static auto FEED = R"({"test":"test"})";

template <typename ExecutionContext>
class NamedForwardFeedTest : public ForwardFeed<ExecutionContext> {
public:
    NamedForwardFeedTest(ExecutionContext& ctx) : ForwardFeed<ExecutionContext>(ctx, "test")
    {
    }
};

using FeedForwardTest = FeedBaseTest<NamedForwardFeedTest>;

TEST_F(FeedForwardTest, Pub)
{
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    auto const json = json::parse(FEED).as_object();

    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(FEED))).Times(1);
    testFeedPtr->pub(json);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    testFeedPtr->pub(json);
}

TEST_F(FeedForwardTest, AutoDisconnect)
{
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    auto const json = json::parse(FEED).as_object();
    EXPECT_CALL(*mockSessionPtr, send(SharedStringJsonEq(FEED))).Times(1);
    testFeedPtr->pub(json);
    sessionPtr.reset();
    EXPECT_EQ(testFeedPtr->count(), 0);
    testFeedPtr->pub(json);
}
