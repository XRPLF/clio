#include "util/AsioContextTestFixture.hpp"
#include "util/Spawn.hpp"
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

    util::spawn(ctx_, [this](boost::asio::yield_context yield) {
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
