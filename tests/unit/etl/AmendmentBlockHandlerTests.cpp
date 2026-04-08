#include "etl/SystemState.hpp"
#include "etl/impl/AmendmentBlockHandler.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/async/context/BasicExecutionContext.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <semaphore>

using namespace etl::impl;

struct AmendmentBlockHandlerTests : util::prometheus::WithPrometheus {
protected:
    testing::StrictMock<testing::MockFunction<void()>> actionMock_;
    etl::SystemState state_;

    util::async::CoroExecutionContext ctx_;
};

TEST_F(AmendmentBlockHandlerTests, CallToNotifyAmendmentBlockedSetsStateAndRepeatedlyCallsAction)
{
    static constexpr auto kMAX_ITERATIONS = 10uz;
    etl::impl::AmendmentBlockHandler handler{
        ctx_, state_, std::chrono::nanoseconds{1}, actionMock_.AsStdFunction()
    };
    auto counter = 0uz;
    std::binary_semaphore stop{0};

    EXPECT_FALSE(state_.isAmendmentBlocked);
    EXPECT_CALL(actionMock_, Call()).Times(testing::AtLeast(10)).WillRepeatedly([&]() {
        if (++counter; counter > kMAX_ITERATIONS)
            stop.release();
    });

    handler.notifyAmendmentBlocked();
    stop.acquire();  // wait for the counter to reach over kMAX_ITERATIONS
    handler.stop();

    EXPECT_TRUE(state_.isAmendmentBlocked);
}

struct DefaultAmendmentBlockActionTest : LoggerFixture {};

TEST_F(DefaultAmendmentBlockActionTest, Call)
{
    AmendmentBlockHandler::kDEFAULT_AMENDMENT_BLOCK_ACTION();
    auto const loggerString = getLoggerString();
    EXPECT_TRUE(loggerString.starts_with("cri:ETL - Can't process new ledgers"))
        << "LoggerString " << loggerString;
}
