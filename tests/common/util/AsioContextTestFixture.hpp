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

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>
#include <gmock/gmock.h>

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
        work.emplace(ctx);  // make sure ctx does not stop on its own
        runner.emplace([&] { ctx.run(); });
    }

    ~AsyncAsioContextTest() override
    {
        work.reset();
        if (runner->joinable())
            runner->join();
        ctx.stop();
    }

    void
    stop()
    {
        work.reset();
        if (runner->joinable())
            runner->join();
        ctx.stop();
    }

protected:
    boost::asio::io_context ctx;

private:
    std::optional<boost::asio::io_service::work> work;
    std::optional<std::thread> runner;
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
    runSpawn(F&& f)
    {
        using namespace boost::asio;

        testing::MockFunction<void()> call;
        spawn(ctx, [&, _ = make_work_guard(ctx)](yield_context yield) {
            f(yield);
            call.Call();
        });

        EXPECT_CALL(call, Call());
        runContext();
    }

    void
    runContext()
    {
        ctx.run();
        ctx.reset();
    }

protected:
    boost::asio::io_context ctx;
};
