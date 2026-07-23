#include "cluster/impl/RepeatedTask.hpp"
#include "util/AsioContextTestFixture.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <semaphore>
#include <thread>

using namespace cluster::impl;
using namespace testing;

struct RepeatedTaskTest : AsyncAsioContextTest {
    static constexpr auto kTimeout = std::chrono::seconds{5};
};

template <typename MockFunctionType>
struct RepeatedTaskTypedTest : RepeatedTaskTest {
    std::atomic_int32_t callCount{0};
    std::binary_semaphore semaphore{0};
    testing::StrictMock<MockFunctionType> mockFn;

    void
    expectCalls(int const expectedCalls)
    {
        callCount = 0;

        EXPECT_CALL(mockFn, Call)
            .Times(AtLeast(expectedCalls))
            .WillRepeatedly([this, expectedCalls](auto&&...) {
                ++callCount;
                if (callCount >= expectedCalls) {
                    semaphore.release();
                }
            });
    }
};

namespace {

using TypesToTest = Types<MockFunction<void()>, MockFunction<void(boost::asio::yield_context)>>;

}  // namespace

TYPED_TEST_SUITE(RepeatedTaskTypedTest, TypesToTest);

TYPED_TEST(RepeatedTaskTypedTest, CallsFunctionRepeatedly)
{
    RepeatedTask<boost::asio::io_context> task(std::chrono::milliseconds(1), this->ctx_);

    this->expectCalls(3);

    task.run(this->mockFn.AsStdFunction());

    EXPECT_TRUE(this->semaphore.try_acquire_for(TestFixture::kTimeout));

    task.stop();
}

TYPED_TEST(RepeatedTaskTypedTest, StopsImmediately)
{
    auto const interval = std::chrono::seconds(5);
    RepeatedTask<boost::asio::io_context> task(interval, this->ctx_);

    task.run(this->mockFn.AsStdFunction());

    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    auto start = std::chrono::steady_clock::now();
    task.stop();
    EXPECT_LT(std::chrono::steady_clock::now() - start, interval);
}

TYPED_TEST(RepeatedTaskTypedTest, MultipleStops)
{
    RepeatedTask<boost::asio::io_context> task(std::chrono::milliseconds(1), this->ctx_);

    this->expectCalls(3);

    task.run(this->mockFn.AsStdFunction());

    EXPECT_TRUE(this->semaphore.try_acquire_for(TestFixture::kTimeout));

    task.stop();
    task.stop();
    task.stop();
}

TYPED_TEST(RepeatedTaskTypedTest, DestructorStopsTask)
{
    this->expectCalls(3);

    {
        RepeatedTask<boost::asio::io_context> task(std::chrono::milliseconds(1), this->ctx_);

        task.run(this->mockFn.AsStdFunction());

        EXPECT_TRUE(this->semaphore.try_acquire_for(TestFixture::kTimeout));

        // Destructor will call stop()
    }

    auto const countAfterDestruction = this->callCount.load();

    // Wait a bit - no more calls should happen
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_EQ(this->callCount, countAfterDestruction);
}

TYPED_TEST(RepeatedTaskTypedTest, StopWithoutRunIsNoOp)
{
    RepeatedTask<boost::asio::io_context> task(std::chrono::milliseconds(1), this->ctx_);

    // Should not crash or hang
    task.stop();
}

TEST_F(RepeatedTaskTest, MultipleTasksRunConcurrently)
{
    StrictMock<MockFunction<void()>> mockFn1;
    StrictMock<MockFunction<void()>> mockFn2;

    RepeatedTask<boost::asio::io_context> task1(std::chrono::milliseconds(1), ctx_);
    RepeatedTask<boost::asio::io_context> task2(std::chrono::milliseconds(2), ctx_);

    std::atomic_int32_t callCount1{0};
    std::atomic_int32_t callCount2{0};
    std::binary_semaphore semaphore1{0};
    std::binary_semaphore semaphore2{0};

    EXPECT_CALL(mockFn1, Call).Times(AtLeast(10)).WillRepeatedly([&]() {
        if (++callCount1 >= 10) {
            semaphore1.release();
        }
    });

    EXPECT_CALL(mockFn2, Call).Times(AtLeast(5)).WillRepeatedly([&]() {
        if (++callCount2 >= 5) {
            semaphore2.release();
        }
    });

    task1.run(mockFn1.AsStdFunction());
    task2.run(mockFn2.AsStdFunction());

    EXPECT_TRUE(semaphore1.try_acquire_for(kTimeout));
    EXPECT_TRUE(semaphore2.try_acquire_for(kTimeout));

    task1.stop();
    task2.stop();
}

TYPED_TEST(RepeatedTaskTypedTest, TaskStateTransitionsCorrectly)
{
    RepeatedTask<boost::asio::io_context> task(std::chrono::milliseconds(1), this->ctx_);

    task.stop();  // Should be no-op

    this->expectCalls(3);

    task.run(this->mockFn.AsStdFunction());

    EXPECT_TRUE(this->semaphore.try_acquire_for(TestFixture::kTimeout));

    task.stop();

    // Stop again should be no-op
    task.stop();
}

TEST_F(RepeatedTaskTest, FunctionCanAccessYieldContext)
{
    StrictMock<MockFunction<void(boost::asio::yield_context)>> mockFn;
    std::atomic_bool yieldContextUsed = false;
    std::binary_semaphore semaphore{0};

    RepeatedTask<boost::asio::io_context> task(std::chrono::milliseconds(1), ctx_);

    EXPECT_CALL(mockFn, Call)
        .Times(AtLeast(1))
        .WillRepeatedly([&](boost::asio::yield_context yield) {
            if (yieldContextUsed)
                return;

            // Use the yield context to verify it's valid
            boost::asio::steady_timer timer(yield.get_executor());
            timer.expires_after(std::chrono::milliseconds(1));
            boost::system::error_code ec;
            timer.async_wait(yield[ec]);
            EXPECT_FALSE(ec) << ec.message();
            yieldContextUsed = true;
            semaphore.release();
        });

    task.run(mockFn.AsStdFunction());

    EXPECT_TRUE(semaphore.try_acquire_for(kTimeout));

    task.stop();

    EXPECT_TRUE(yieldContextUsed);
}
