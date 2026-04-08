#include "util/AsioContextTestFixture.hpp"
#include "util/CallWithTimeout.hpp"
#include "util/Repeat.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <ranges>
#include <thread>
#include <utility>

using namespace util;
using testing::AtLeast;

struct RepeatTests : SyncAsioContextTest {
    Repeat repeat{ctx_};
    testing::StrictMock<testing::MockFunction<void()>> handlerMock;

    void
    withRunningContext(std::function<void()> func)
    {
        tests::common::util::callWithTimeout(
            std::chrono::seconds{1}, [this, func = std::move(func)]() {
                auto workGuard = boost::asio::make_work_guard(ctx_);
                std::thread thread{[this]() { ctx_.run(); }};
                func();
                workGuard.reset();
                thread.join();
            }
        );
    }
};

TEST_F(RepeatTests, CallsHandler)
{
    EXPECT_CALL(handlerMock, Call).Times(testing::AtMost(22));
    repeat.start(std::chrono::milliseconds{1}, handlerMock.AsStdFunction());
    runContextFor(std::chrono::milliseconds{20});
}

TEST_F(RepeatTests, StopsOnStop)
{
    withRunningContext([this]() {
        EXPECT_CALL(handlerMock, Call).Times(AtLeast(1));
        repeat.start(std::chrono::milliseconds{1}, handlerMock.AsStdFunction());
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        repeat.stop();
    });
}

TEST_F(RepeatTests, RunsAfterStop)
{
    withRunningContext([this]() {
        for ([[maybe_unused]] auto i : std::ranges::iota_view(0, 2)) {
            EXPECT_CALL(handlerMock, Call).Times(AtLeast(1));
            repeat.start(std::chrono::milliseconds{1}, handlerMock.AsStdFunction());
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            repeat.stop();
        }
    });
}
