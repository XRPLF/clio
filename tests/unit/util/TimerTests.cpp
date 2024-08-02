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

#include "util/AsioContextTestFixture.hpp"
#include "util/Timer.hpp"
#include "util/WithTimeout.hpp"

#include <boost/asio/error.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

using namespace util;

struct TimerTests : SyncAsioContextTest {
    Timer timer{ctx};
    testing::StrictMock<testing::MockFunction<void(boost::system::error_code)>> handlerMock;
};

TEST_F(TimerTests, AsyncWaitCallsHandler)
{
    timer.expires_after(std::chrono::milliseconds{10});
    timer.async_wait(handlerMock.AsStdFunction());
    EXPECT_CALL(handlerMock, Call(boost::system::error_code{}));
    ctx.run();
}

TEST_F(TimerTests, CancelCancelsTimer)
{
    timer.expires_after(std::chrono::milliseconds{10});
    timer.async_wait(handlerMock.AsStdFunction());
    timer.cancel();
    EXPECT_CALL(handlerMock, Call(boost::system::error_code{boost::asio::error::operation_aborted}));
    ctx.run();
}

TEST_F(TimerTests, RepeatingTimerCanBeDestroyedWhileIoContextIsRunning)
{
    auto timerPtr = std::make_unique<Timer>(ctx);

    EXPECT_CALL(handlerMock, Call(boost::system::error_code{})).WillRepeatedly([&](auto const& ec) {
        if (ec == boost::asio::error::operation_aborted)
            return;
        timerPtr->expires_after(std::chrono::milliseconds{1});
        timerPtr->async_wait(handlerMock.AsStdFunction());
    });
    timerPtr->expires_after(std::chrono::milliseconds{1});
    timerPtr->async_wait(handlerMock.AsStdFunction());

    std::thread thread{[this]() { ctx.run(); }};
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    timerPtr.reset();
    tests::common::util::withTimeout(std::chrono::seconds{1}, [&thread]() { thread.join(); });
}
