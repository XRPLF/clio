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
#include "util/Repeat.hpp"
#include "util/WithTimeout.hpp"

#include <boost/asio/error.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <thread>

using namespace util;

struct TimerTests : SyncAsioContextTest {
    Repeat timer{ctx};
    testing::StrictMock<testing::MockFunction<void(boost::system::error_code)>> handlerMock;
};

/*TEST_F(TimerTests, AsyncWaitCallsHandler)*/
/*{*/
/*    timer.expires_after(std::chrono::milliseconds{10});*/
/*    timer.async_wait(handlerMock.AsStdFunction());*/
/*    EXPECT_CALL(handlerMock, Call(boost::system::error_code{}));*/
/*    ctx.run();*/
/*}*/
/**/
TEST_F(TimerTests, CancelCancelsTimer)
{
    timer.expires_after(std::chrono::milliseconds{10});
    timer.async_wait(handlerMock.AsStdFunction());
    timer.cancel();
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    ctx.run();
}

TEST_F(TimerTests, RepeatingTimerCanBeDestroyedWhileIoContextIsRunning)
{
    auto workGuard = boost::asio::make_work_guard(ctx);
    std::promise<void> finished;
    std::thread thread{[&]() {
        ctx.run();
        finished.set_value();
    }};
    {
        Repeat timer(ctx);

        EXPECT_CALL(handlerMock, Call).WillRepeatedly([&](auto const&) {
            timer.expires_after(std::chrono::nanoseconds{1});
            timer.async_wait(handlerMock.AsStdFunction());
        });
        timer.expires_after(std::chrono::nanoseconds{1});
        timer.async_wait(handlerMock.AsStdFunction());

        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    workGuard.reset();

    if (finished.get_future().wait_for(std::chrono::seconds{1}) == std::future_status::timeout)
        FAIL() << "Timer was not destroyed";
    thread.join();
}

TEST_F(TimerTests, Cancel)
{
    auto workGuard = boost::asio::make_work_guard(ctx);
    std::promise<void> finished;
    std::thread thread{[&]() {
        ctx.run();
        finished.set_value();
    }};
    {
        Repeat timer(ctx);

        size_t counter = 0;
        EXPECT_CALL(handlerMock, Call).WillRepeatedly([&](auto const&) {
            timer.expires_after(std::chrono::nanoseconds{1});
            timer.async_wait(handlerMock.AsStdFunction());
            ++counter;
            if (counter == 10) {
                timer.cancel();
                counter = 0;
                timer.expires_after(std::chrono::nanoseconds{1});
                timer.async_wait(handlerMock.AsStdFunction());
            }
        });
        timer.expires_after(std::chrono::nanoseconds{1});
        timer.async_wait(handlerMock.AsStdFunction());

        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    workGuard.reset();

    if (finished.get_future().wait_for(std::chrono::seconds{1}) == std::future_status::timeout)
        FAIL() << "Timer was not destroyed";
    thread.join();
}
