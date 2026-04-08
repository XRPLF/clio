#include "util/AsioContextTestFixture.hpp"
#include "util/Coroutine.hpp"
#include "util/Profiler.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <ranges>
#include <string>

using namespace util;

class CoroutineTest : public SyncAsioContextTest {
protected:
    testing::StrictMock<testing::MockFunction<void(Coroutine&)>> fnMock_;

    static void
    asyncOperation(
        Coroutine::cancellable_yield_context_type yield,
        std::chrono::steady_clock::duration duration
    )
    {
        boost::asio::steady_timer timer(yield.get().get_executor(), duration);
        timer.async_wait(yield);
    }
};

TEST_F(CoroutineTest, SpawnNew)
{
    EXPECT_CALL(fnMock_, Call);

    Coroutine::spawnNew(ctx_, fnMock_.AsStdFunction());
    ctx_.run();
}

TEST_F(CoroutineTest, SpawnChild)
{
    EXPECT_CALL(fnMock_, Call);
    runCoroutine([this](Coroutine& coroutine) { coroutine.spawnChild(fnMock_.AsStdFunction()); });
}

TEST_F(CoroutineTest, SpawnChildDoesNothingWhenTheCoroutineIsCancelled)
{
    runCoroutine([this](Coroutine& coroutine) {
        coroutine.cancelAll();
        coroutine.spawnChild(fnMock_.AsStdFunction());
    });
}

TEST_F(CoroutineTest, ErrorReturnsDefaultWhenNoError)
{
    runCoroutine([](Coroutine& coroutine) {
        EXPECT_EQ(coroutine.error(), boost::system::error_code{});
    });
}

TEST_F(CoroutineTest, ErrorReturnsDefaultAfterSuccessfulOperation)
{
    runCoroutine([](Coroutine& coroutine) {
        boost::asio::steady_timer timer(coroutine.executor());
        timer.expires_after(std::chrono::milliseconds(1));
        timer.async_wait(coroutine.yieldContext());
        EXPECT_EQ(coroutine.error(), boost::system::error_code{});
    });
}

TEST_F(CoroutineTest, ErrorReturnsErrorOfLastOperation)
{
    runCoroutine([](Coroutine& coroutine) {
        boost::asio::ip::tcp::socket socket{coroutine.executor()};
        std::string buffer;
        socket.async_read_some(boost::asio::buffer(buffer), coroutine.yieldContext());
        EXPECT_EQ(coroutine.error(), boost::system::errc::bad_file_descriptor);
    });
}

TEST_F(CoroutineTest, CancelAllCancelsChildren)
{
    runCoroutine([&](Coroutine& coroutine) {
        coroutine.spawnChild([](Coroutine& childCoroutine) {
            auto const duration = util::timed([&childCoroutine]() {
                asyncOperation(childCoroutine.yieldContext(), std::chrono::seconds{5});
            });
            EXPECT_TRUE(childCoroutine.isCancelled());
            EXPECT_LT(duration, 1000);
        });
        coroutine.cancelAll();
        EXPECT_TRUE(coroutine.isCancelled());
    });
}

TEST_F(CoroutineTest, CancelAllCancelsParent)
{
    runCoroutine([&](Coroutine& coroutine) {
        coroutine.spawnChild([](Coroutine& childCoroutine) {
            childCoroutine.yield();
            childCoroutine.cancelAll();
            EXPECT_TRUE(childCoroutine.isCancelled());
        });

        auto const duration = util::timed([&coroutine]() {
            asyncOperation(coroutine.yieldContext(), std::chrono::seconds{5});
        });
        EXPECT_TRUE(coroutine.isCancelled());
        EXPECT_LT(duration, 1000);
    });
}

TEST_F(CoroutineTest, CancelAllCalledMultipleTimes)
{
    runCoroutine([&](Coroutine& coroutine) {
        coroutine.spawnChild([](Coroutine& childCoroutine) {
            childCoroutine.yield();
            for ([[maybe_unused]] auto const i : std::ranges::iota_view(0, 10)) {
                childCoroutine.cancelAll();
            }
            EXPECT_TRUE(childCoroutine.isCancelled());
        });

        auto const duration = util::timed([&coroutine]() {
            asyncOperation(coroutine.yieldContext(), std::chrono::seconds{5});
        });
        EXPECT_TRUE(coroutine.isCancelled());
        EXPECT_LT(duration, 1000);
    });
}

TEST_F(CoroutineTest, CancelAllCancelsSiblingsAndParent)
{
    EXPECT_CALL(fnMock_, Call).Times(2);
    runCoroutine([&](Coroutine& parentCoroutine) {
        parentCoroutine.spawnChild([&](Coroutine& child1Coroutine) {
            auto duration = util::timed([&child1Coroutine]() {
                asyncOperation(child1Coroutine.yieldContext(), std::chrono::seconds(5));
            });
            EXPECT_TRUE(child1Coroutine.isCancelled());
            EXPECT_LT(duration, 2000);
            EXPECT_EQ(child1Coroutine.error(), boost::asio::error::operation_aborted);
            fnMock_.Call(child1Coroutine);
        });
        parentCoroutine.spawnChild([&](Coroutine& child2Coroutine) {
            child2Coroutine.yield();
            child2Coroutine.cancelAll();
            fnMock_.Call(child2Coroutine);
        });

        auto parentDuration = util::timed([&parentCoroutine]() {
            asyncOperation(parentCoroutine.yieldContext(), std::chrono::seconds(5));
        });
        EXPECT_TRUE(parentCoroutine.isCancelled());
        EXPECT_LT(parentDuration, 2000);
        EXPECT_EQ(parentCoroutine.error(), boost::asio::error::operation_aborted);
    });
}

TEST_F(CoroutineTest, Yield)
{
    testing::StrictMock<testing::MockFunction<void()>> anotherFnMock;
    testing::Sequence const sequence;
    EXPECT_CALL(fnMock_, Call).InSequence(sequence);
    EXPECT_CALL(anotherFnMock, Call).InSequence(sequence);

    runCoroutine([&](Coroutine& coroutine) {
        coroutine.spawnChild([&anotherFnMock](Coroutine& childCoroutine) {
            childCoroutine.yield();
            anotherFnMock.Call();
        });
        fnMock_.Call(coroutine);
    });
}
