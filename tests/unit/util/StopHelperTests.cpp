//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "util/AsioContextTestFixture.hpp"
#include "util/StopHelper.hpp"

#include <boost/asio/spawn.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace util;

struct StopHelperTests : SyncAsioContextTest {
protected:
    StopHelper stopHelper_;
    testing::StrictMock<testing::MockFunction<void()>> readyToStopCalled_;
    testing::StrictMock<testing::MockFunction<void()>> asyncWaitForStopFinished_;
};

TEST_F(StopHelperTests, asyncWaitForStopWaitsForReadyToStop)
{
    testing::Sequence const sequence;
    EXPECT_CALL(readyToStopCalled_, Call).InSequence(sequence);
    EXPECT_CALL(asyncWaitForStopFinished_, Call).InSequence(sequence);

    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        stopHelper_.asyncWaitForStop(yield);
        asyncWaitForStopFinished_.Call();
    });

    runSpawn([this](auto&&) {
        stopHelper_.readyToStop();
        readyToStopCalled_.Call();
    });
}

TEST_F(StopHelperTests, readyToStopCalledBeforeAsyncWait)
{
    stopHelper_.readyToStop();
    EXPECT_CALL(asyncWaitForStopFinished_, Call);
    runSpawn([this](boost::asio::yield_context yield) {
        stopHelper_.asyncWaitForStop(yield);
        asyncWaitForStopFinished_.Call();
    });
}
