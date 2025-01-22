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

#pragma once

#include "util/LoggerFixtures.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <thread>

/**
 * @brief Fixture with an embedded boost::asio context running on a thread
 *
 * This is meant to be used as a base for other fixtures.
 */
struct AsyncAsioContextTest : virtual public NoLoggerFixture {
    AsyncAsioContextTest()
    {
        work_.emplace(ctx_);  // make sure ctx does not stop on its own
        runner_.emplace([&] { ctx_.run(); });
    }

    ~AsyncAsioContextTest() override
    {
        work_.reset();
        if (runner_->joinable())
            runner_->join();
        ctx_.stop();
    }

    void
    stop()
    {
        work_.reset();
        if (runner_->joinable())
            runner_->join();
        ctx_.stop();
    }

protected:
    boost::asio::io_context ctx_;

private:
    std::optional<boost::asio::io_service::work> work_;
    std::optional<std::thread> runner_;
};

/**
 * @brief Fixture with an embedded boost::asio context that is not running by
 * default but can be progressed on the calling thread
 *
 * Use `run_for(duration)` etc. directly on `ctx`.
 * This is meant to be used as a base for other fixtures.
 */
struct SyncAsioContextTest : virtual public NoLoggerFixture {
    template <typename F>
    void
    runSpawn(F&& f, bool allowMockLeak = false)
    {
        using namespace boost::asio;

        testing::MockFunction<void()> call;
        if (allowMockLeak)
            testing::Mock::AllowLeak(&call);

        spawn(ctx_, [&, _ = make_work_guard(ctx_)](yield_context yield) {
            f(yield);
            call.Call();
        });

        EXPECT_CALL(call, Call());
        runContext();
    }

    template <typename F>
    void
    runSpawnWithTimeout(std::chrono::steady_clock::duration timeout, F&& f, bool allowMockLeak = false)
    {
        using namespace boost::asio;

        boost::asio::io_context timerCtx;
        steady_timer timer{timerCtx, timeout};
        spawn(timerCtx, [this, &timer](yield_context yield) {
            boost::system::error_code errorCode;
            timer.async_wait(yield[errorCode]);
            ctx_.stop();
            EXPECT_TRUE(false) << "Test timed out";
        });
        std::thread timerThread{[&timerCtx]() { timerCtx.run(); }};

        testing::MockFunction<void()> call;
        if (allowMockLeak)
            testing::Mock::AllowLeak(&call);

        spawn(ctx_, [&](yield_context yield) {
            f(yield);
            call.Call();
        });

        EXPECT_CALL(call, Call());
        runContext();

        timerCtx.stop();
        timerThread.join();
    }

    void
    runContext()
    {
        ctx_.run();
        ctx_.reset();
    }

    void
    runContextFor(std::chrono::milliseconds duration)
    {
        ctx_.run_for(duration);
        ctx_.reset();
    }

    template <typename F>
    static void
    runSyncOperation(F&& f)
    {
        boost::asio::io_service ioc;
        boost::asio::spawn(ioc, f);
        ioc.run();
    }

protected:
    boost::asio::io_context ctx_;
};
