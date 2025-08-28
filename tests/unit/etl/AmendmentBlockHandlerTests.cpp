//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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
#include "etl/impl/AmendmentBlockHandler.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>

using namespace etl::impl;

struct AmendmentBlockHandlerTest : util::prometheus::WithPrometheus, SyncAsioContextTest {
    testing::StrictMock<testing::MockFunction<void()>> actionMock;
    etl::SystemState state;
};

TEST_F(AmendmentBlockHandlerTest, CallTonotifyAmendmentBlockedSetsStateAndRepeatedlyCallsAction)
{
    AmendmentBlockHandler handler{ctx_, state, std::chrono::nanoseconds{1}, actionMock.AsStdFunction()};

    EXPECT_FALSE(state.isAmendmentBlocked);
    EXPECT_CALL(actionMock, Call()).Times(testing::AtLeast(10));
    handler.notifyAmendmentBlocked();
    EXPECT_TRUE(state.isAmendmentBlocked);

// Code runs significantly slower when assertions are enabled
#ifdef _GLIBCXX_ASSERTIONS
    runContextFor(std::chrono::milliseconds{10});
#else
    runContextFor(std::chrono::milliseconds{1});
#endif
}

struct DefaultAmendmentBlockActionTest : LoggerFixture {};

TEST_F(DefaultAmendmentBlockActionTest, Call)
{
    AmendmentBlockHandler::kDEFAULT_AMENDMENT_BLOCK_ACTION();
    auto const loggerString = getLoggerString();
    EXPECT_TRUE(loggerString.starts_with("cri:ETL - Can't process new ledgers")) << "LoggerString " << loggerString;
}
