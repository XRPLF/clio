#pragma once

#include "util/Coroutine.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/Spawn.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
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
struct AsyncAsioContextTest : virtual public ::testing::Test {
    AsyncAsioContextTest()
    {
        work_.emplace(
            boost::asio::make_work_guard(ctx_)
        );  // make sure ctx does not stop on its own
        runner_.emplace([&] { ctx_.run(); });
    }

    ~AsyncAsioContextTest() override
    {
        work_.reset();
        if (runner_.has_value() && runner_->joinable())
            runner_->join();
        ctx_.stop();
    }

    void
    stop()
    {
        work_.reset();
        if (runner_.has_value() && runner_->joinable())
            runner_->join();
        ctx_.stop();
    }

protected:
    boost::asio::io_context ctx_;

private:
    std::optional<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_;
    std::optional<std::thread> runner_;
};

/**
 * @brief Fixture with an embedded boost::asio context that is not running by
 * default but can be progressed on the calling thread
 *
 * Use `run_for(duration)` etc. directly on `ctx`.
 * This is meant to be used as a base for other fixtures.
 */
struct SyncAsioContextTest : virtual public ::testing::Test {
    template <util::CoroutineFunction F>
    void
    runCoroutine(F&& f, bool allowMockLeak = false)
    {
        testing::MockFunction<void()> call;
        if (allowMockLeak)
            testing::Mock::AllowLeak(&call);

        util::Coroutine::spawnNew(
            ctx_, [&, _ = boost::asio::make_work_guard(ctx_)](util::Coroutine& coroutine) {
                f(coroutine);
                call.Call();
            }
        );
        EXPECT_CALL(call, Call());
        runContext();
    }

    template <typename F>
    void
    runSpawn(F&& f, bool allowMockLeak = false)
    {
        testing::MockFunction<void()> call;
        if (allowMockLeak)
            testing::Mock::AllowLeak(&call);

        util::spawn(ctx_, [&, _ = make_work_guard(ctx_)](boost::asio::yield_context yield) {
            f(yield);
            call.Call();
        });

        EXPECT_CALL(call, Call());
        runContext();
    }

    void
    runContext()
    {
        static constexpr auto kTimeout = std::chrono::seconds{30};
        ctx_.run_for(kTimeout);
        ctx_.restart();
    }

    void
    runContextFor(std::chrono::milliseconds duration)
    {
        ctx_.run_for(duration);
        ctx_.restart();
    }

    template <typename F>
    static void
    runSyncOperation(F&& f)
    {
        boost::asio::io_context ioc;
        util::spawn(ioc, f);
        ioc.run();
    }

protected:
    boost::asio::io_context ctx_;
};
