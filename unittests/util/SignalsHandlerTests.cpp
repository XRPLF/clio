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

#include "util/SignalsHandler.hpp"
#include "util/config/Config.hpp"

#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <string>
#include <thread>

using namespace util;
using testing::MockFunction;
using testing::StrictMock;

struct SignalsHandlerTestsBase : ::testing::Test {
    StrictMock<MockFunction<void(std::string)>> forceExitHandler_;
    StrictMock<MockFunction<void()>> stopHandler_;
    StrictMock<MockFunction<void()>> anotherStopHandler_;
};

TEST(SignalsHandlerDeathTest, CantCreateTwoSignalsHandlers)
{
    auto makeHandler = []() { return SignalsHandler{Config{}, [](std::string) {}}; };
    auto const handler = makeHandler();
    EXPECT_DEATH({ makeHandler(); }, ".*");
}

struct SignalsHandlerTests : SignalsHandlerTestsBase {
    SignalsHandler handler_{
        util::Config{boost::json::value{{"graceful_period", 0.05}}},
        forceExitHandler_.AsStdFunction()
    };
};

TEST_F(SignalsHandlerTests, NoSignal)
{
    handler_.subscribeToStop(stopHandler_.AsStdFunction());
    handler_.subscribeToStop(anotherStopHandler_.AsStdFunction());
}

TEST_F(SignalsHandlerTests, OneSignal)
{
    handler_.subscribeToStop(stopHandler_.AsStdFunction());
    handler_.subscribeToStop(anotherStopHandler_.AsStdFunction());

    EXPECT_CALL(stopHandler_, Call());
    EXPECT_CALL(anotherStopHandler_, Call());
    std::raise(SIGINT);
}

struct SignalsHandlerTimeoutTests : SignalsHandlerTestsBase {
    SignalsHandler handler_{
        util::Config{boost::json::value{{"graceful_period", 0.001}}},
        forceExitHandler_.AsStdFunction()
    };
};

TEST_F(SignalsHandlerTimeoutTests, OneSignalTimeout)
{
    handler_.subscribeToStop(stopHandler_.AsStdFunction());
    EXPECT_CALL(stopHandler_, Call()).WillOnce([] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); });
    EXPECT_CALL(forceExitHandler_, Call("Force exit at the end of graceful period."));
    std::raise(SIGINT);
}

TEST_F(SignalsHandlerTests, TwoSignals)
{
    handler_.subscribeToStop(stopHandler_.AsStdFunction());
    EXPECT_CALL(stopHandler_, Call()).WillOnce([] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); });
    EXPECT_CALL(forceExitHandler_, Call("Force exit on second signal."));
    std::raise(SIGINT);
    std::raise(SIGTERM);
}

struct SignalsHandlerPriorityTestsBundle {
    std::string name;
    SignalsHandler::Priority stopHandlerPriority;
    SignalsHandler::Priority anotherStopHandlerPriority;
};

struct SignalsHandlerPriorityTests : SignalsHandlerTestsBase,
                                     testing::WithParamInterface<SignalsHandlerPriorityTestsBundle> {
    SignalsHandler handler_{
        util::Config{boost::json::value{{"graceful_period", 0.05}}},
        forceExitHandler_.AsStdFunction()
    };
};
INSTANTIATE_TEST_SUITE_P(
    SignalsHandlerPriorityTestsGroup,
    SignalsHandlerPriorityTests,
    testing::Values(
        SignalsHandlerPriorityTestsBundle{
            "StopFirst-Normal",
            SignalsHandler::Priority::StopFirst,
            SignalsHandler::Priority::Normal
        },
        SignalsHandlerPriorityTestsBundle{
            "Normal-StopLast",
            SignalsHandler::Priority::Normal,
            SignalsHandler::Priority::StopLast
        }
    )
);

TEST_P(SignalsHandlerPriorityTests, Priority)
{
    bool stopHandlerCalled = false;

    handler_.subscribeToStop(anotherStopHandler_.AsStdFunction(), GetParam().anotherStopHandlerPriority);
    handler_.subscribeToStop(stopHandler_.AsStdFunction(), GetParam().stopHandlerPriority);

    EXPECT_CALL(stopHandler_, Call()).WillOnce([&] { stopHandlerCalled = true; });
    EXPECT_CALL(anotherStopHandler_, Call()).WillOnce([&] { EXPECT_TRUE(stopHandlerCalled); });
    std::raise(SIGINT);
}

// no signal test
// one signal no timeout test
// one signal timeout test
// two signals test
// priority test
