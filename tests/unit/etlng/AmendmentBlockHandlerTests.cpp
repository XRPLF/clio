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

#include "etl/SystemState.hpp"
#include "etlng/impl/AmendmentBlockHandler.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/async/context/BasicExecutionContext.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <semaphore>

using namespace etlng::impl;

struct AmendmentBlockHandlerNgTests : util::prometheus::WithPrometheus {
protected:
    testing::StrictMock<testing::MockFunction<void()>> actionMock_;
    etl::SystemState state_;

    util::async::CoroExecutionContext ctx_;
};

TEST_F(AmendmentBlockHandlerNgTests, CallTonotifyAmendmentBlockedSetsStateAndRepeatedlyCallsAction)
{
    constexpr static auto kMAX_ITERATIONS = 10uz;
    etlng::impl::AmendmentBlockHandler handler{ctx_, state_, std::chrono::nanoseconds{1}, actionMock_.AsStdFunction()};
    auto counter = 0uz;
    std::binary_semaphore stop{0};

    EXPECT_FALSE(state_.isAmendmentBlocked);
    EXPECT_CALL(actionMock_, Call()).Times(testing::AtLeast(10)).WillRepeatedly([&]() {
        if (++counter; counter > kMAX_ITERATIONS)
            stop.release();
    });

    handler.notifyAmendmentBlocked();
    stop.acquire();  // wait for the counter to reach over kMAX_ITERATIONS

    EXPECT_TRUE(state_.isAmendmentBlocked);
}

struct DefaultAmendmentBlockActionNgTest : LoggerFixture {};

TEST_F(DefaultAmendmentBlockActionNgTest, Call)
{
    AmendmentBlockHandler::kDEFAULT_AMENDMENT_BLOCK_ACTION();
    auto const loggerString = getLoggerString();
    EXPECT_TRUE(loggerString.starts_with("ETL:FTL Can't process new ledgers")) << "LoggerString " << loggerString;
}
