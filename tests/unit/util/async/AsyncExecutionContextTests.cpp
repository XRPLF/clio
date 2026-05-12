#include "util/MockAssert.hpp"
#include "util/Profiler.hpp"
#include "util/async/Operation.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/async/context/SyncExecutionContext.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <ranges>
#include <semaphore>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace util::async;
using ::testing::Types;

template <typename T>
struct ExecutionContextTests : common::util::WithMockAssertNoThrow {
    using ExecutionContextType = T;
    ExecutionContextType ctx{2};

    ~ExecutionContextTests() override
    {
        ctx.stop();
        ctx.join();
    }
};

// Suite for tests to be ran against all context types but SyncExecutionContext
template <typename T>
using AsyncExecutionContextTests = ExecutionContextTests<T>;

using ExecutionContextTypes =
    Types<CoroExecutionContext, PoolExecutionContext, SyncExecutionContext>;
using AsyncExecutionContextTypes = Types<CoroExecutionContext, PoolExecutionContext>;

TYPED_TEST_CASE(ExecutionContextTests, ExecutionContextTypes);
TYPED_TEST_CASE(AsyncExecutionContextTests, AsyncExecutionContextTypes);

TYPED_TEST(ExecutionContextTests, move)
{
    auto mineNow = std::move(this->ctx);
    EXPECT_TRUE(mineNow.execute([] { return true; }).get().value());
}

TYPED_TEST(ExecutionContextTests, execute)
{
    auto res = this->ctx.execute([]() { return 42; });
    EXPECT_EQ(res.get().value(), 42);
}

TYPED_TEST(ExecutionContextTests, executeVoid)
{
    auto value = 0;
    auto res = this->ctx.execute([&value]() { value = 42; });

    res.wait();
    ASSERT_EQ(value, 42);
}

TYPED_TEST(ExecutionContextTests, executeStdException)
{
    auto res = this->ctx.execute([]() { throw std::runtime_error("test"); });

    auto const err = res.get().error();
    EXPECT_TRUE(err.message.ends_with("test"));
    EXPECT_TRUE(std::string{err}.ends_with("test"));
}

TYPED_TEST(ExecutionContextTests, executeUnknownException)
{
    auto res = this->ctx.execute([]() { throw 0; });

    auto const err = res.get().error();
    EXPECT_TRUE(err.message.ends_with("unknown"));
    EXPECT_TRUE(std::string{err}.ends_with("unknown"));
}

// note: this fails on pool context with 1 thread
TYPED_TEST(ExecutionContextTests, executeWithTimeout)
{
    auto res = this->ctx.execute(
        [](auto stopRequested) {
            while (not stopRequested)
                ;
            return 42;
        },
        std::chrono::milliseconds{1}
    );

    EXPECT_EQ(res.get().value(), 42);
}

TYPED_TEST(ExecutionContextTests, timer)
{
    auto res = this->ctx.scheduleAfter(
        std::chrono::milliseconds(1), []([[maybe_unused]] auto stopRequested, auto cancelled) {
            if (not cancelled)
                return 42;
            return 0;
        }
    );

    EXPECT_EQ(res.get().value(), 42);
}

TYPED_TEST(ExecutionContextTests, timerWithStopToken)
{
    auto res = this->ctx.scheduleAfter(std::chrono::milliseconds(1), [](auto stopRequested) {
        while (not stopRequested)
            ;

        return 42;
    });

    res.requestStop();
    EXPECT_EQ(res.get().value(), 42);
}

TYPED_TEST(ExecutionContextTests, timerCancel)
{
    auto value = 0;
    std::binary_semaphore sem{0};

    auto res = this->ctx.scheduleAfter(
        std::chrono::milliseconds(10),
        [&value, &sem]([[maybe_unused]] auto stopRequested, auto cancelled) {
            if (cancelled)
                value = 42;

            sem.release();
        }
    );

    res.cancel();
    sem.acquire();
    EXPECT_EQ(value, 42);
}

TYPED_TEST(ExecutionContextTests, timerAutoCancels)
{
    auto value = 0;
    std::binary_semaphore sem{0};
    {
        auto res = this->ctx.scheduleAfter(
            std::chrono::milliseconds(1),
            [&value, &sem]([[maybe_unused]] auto stopRequested, auto cancelled) {
                if (cancelled)
                    value = 42;

                sem.release();
            }
        );
    }  // res goes out of scope and cancels the timer

    sem.acquire();
    EXPECT_EQ(value, 42);
}

TYPED_TEST(ExecutionContextTests, timerStdException)
{
    auto res = this->ctx.scheduleAfter(
        std::chrono::milliseconds(1), []([[maybe_unused]] auto stopRequested, auto cancelled) {
            if (not cancelled)
                throw std::runtime_error("test");
            return 0;
        }
    );

    auto const err = res.get().error();
    EXPECT_TRUE(err.message.ends_with("test"));
    EXPECT_TRUE(std::string{err}.ends_with("test"));
}

TYPED_TEST(ExecutionContextTests, timerUnknownException)
{
    auto res = this->ctx.scheduleAfter(
        std::chrono::milliseconds(1), []([[maybe_unused]] auto stopRequested, auto cancelled) {
            if (not cancelled)
                throw 0;
            return 0;
        }
    );

    auto const err = res.get().error();
    EXPECT_TRUE(err.message.ends_with("unknown"));
    EXPECT_TRUE(std::string{err}.ends_with("unknown"));
}

TYPED_TEST(ExecutionContextTests, repeatingOperation)
{
    auto const repeatDelay = std::chrono::milliseconds{1};
    auto const timeout = std::chrono::milliseconds{15};
    std::atomic_size_t callCount = 0uz;

    auto res = this->ctx.executeRepeatedly(repeatDelay, [&] { ++callCount; });
    auto timeSpent = util::timed([timeout] {
        std::this_thread::sleep_for(timeout);
    });  // calculate actual time spent

    res.abort();  // outside of the above stopwatch because it blocks and can take arbitrary time
    auto const expectedPureCalls = timeout.count() / repeatDelay.count();
    auto const expectedActualCount = timeSpent / repeatDelay.count();

    EXPECT_GE(callCount, expectedPureCalls / 2u);  // expect at least half of the scheduled calls
    EXPECT_LE(
        callCount, expectedActualCount
    );  // never should be called more times than possible before timeout
}

TYPED_TEST(ExecutionContextTests, repeatingOperationForceInvoke)
{
    std::atomic_size_t callCount = 64uz;
    std::binary_semaphore unblock(0);

    auto res = this->ctx.executeRepeatedly(std::chrono::seconds{10}, [&] {
        if (--callCount == 0uz)
            unblock.release();
    });
    for ([[maybe_unused]] auto unused : std::views::iota(0uz, callCount.load()))
        res.invoke();

    unblock.acquire();
    res.abort();

    EXPECT_EQ(callCount, 0uz);
}

TYPED_TEST(ExecutionContextTests, submit)
{
    EXPECT_CLIO_ASSERT_FAIL(this->ctx.submit([] -> void { throw 0; }));

    std::atomic_uint32_t count = 0;
    std::binary_semaphore sem{0};

    static constexpr auto kNUM_SUBMISSIONS = 1024;

    for (auto i = 1; i <= kNUM_SUBMISSIONS; ++i) {
        if (i == kNUM_SUBMISSIONS) {
            this->ctx.submit([&count, &sem] {
                ++count;
                sem.release();
            });
        } else {
            this->ctx.submit([&count] { ++count; });
        }
    }

    sem.acquire();

    // order is not guaranteed (see `strandSubmit` below)
    ASSERT_EQ(count, static_cast<size_t>(kNUM_SUBMISSIONS));
}

TYPED_TEST(ExecutionContextTests, strandMove)
{
    auto strand = this->ctx.makeStrand();
    auto yoink = std::move(strand);
    auto res = yoink.execute([] { return 42; });

    EXPECT_EQ(res.get().value(), 42);
}

TYPED_TEST(ExecutionContextTests, strand)
{
    auto strand = this->ctx.makeStrand();
    auto res = strand.execute([] { return 42; });

    EXPECT_EQ(res.get().value(), 42);
}

TYPED_TEST(ExecutionContextTests, strandStdException)
{
    auto strand = this->ctx.makeStrand();
    auto res = strand.execute([]() { throw std::runtime_error("test"); });

    auto const err = res.get().error();
    EXPECT_TRUE(err.message.ends_with("test"));
    EXPECT_TRUE(std::string{err}.ends_with("test"));
}

TYPED_TEST(ExecutionContextTests, strandUnknownException)
{
    auto strand = this->ctx.makeStrand();
    auto res = strand.execute([]() { throw 0; });

    auto const err = res.get().error();
    EXPECT_TRUE(err.message.ends_with("unknown"));
    EXPECT_TRUE(std::string{err}.ends_with("unknown"));
}

// note: this fails on pool context with 1 thread
TYPED_TEST(ExecutionContextTests, strandWithTimeout)
{
    auto strand = this->ctx.makeStrand();
    auto res = strand.execute(
        [](auto stopRequested) {
            while (not stopRequested)
                ;
            return 42;
        },
        std::chrono::milliseconds{1}
    );

    EXPECT_EQ(res.get().value(), 42);
}

TYPED_TEST(ExecutionContextTests, strandedRepeatingOperation)
{
    auto strand = this->ctx.makeStrand();
    auto const repeatDelay = std::chrono::milliseconds{1};
    auto const timeout = std::chrono::milliseconds{15};
    auto callCount = 0uz;

    auto res = strand.executeRepeatedly(repeatDelay, [&] { ++callCount; });
    auto timeSpent = util::timed([timeout] {
        std::this_thread::sleep_for(timeout);
    });  // calculate actual time spent

    res.abort();  // outside of the above stopwatch because it blocks and can take arbitrary time
    auto const expectedPureCalls = timeout.count() / repeatDelay.count();
    auto const expectedActualCount = timeSpent / repeatDelay.count();

    EXPECT_GE(callCount, expectedPureCalls / 2u);  // expect at least half of the scheduled calls
    EXPECT_LE(
        callCount, expectedActualCount
    );  // never should be called more times than possible before timeout
}

TYPED_TEST(ExecutionContextTests, strandedRepeatingOperationForceInvoke)
{
    auto strand = this->ctx.makeStrand();
    auto callCount = 64uz;  // does not need to be atomic since we are on a strand
    std::binary_semaphore unblock(0);

    auto res = strand.executeRepeatedly(std::chrono::seconds{10}, [&] {
        if (--callCount == 0uz)
            unblock.release();
    });
    for ([[maybe_unused]] auto unused : std::views::iota(0uz, callCount))
        res.invoke();

    unblock.acquire();
    res.abort();

    EXPECT_EQ(callCount, 0uz);
}

TYPED_TEST(ExecutionContextTests, strandSubmit)
{
    auto strand = this->ctx.makeStrand();
    EXPECT_CLIO_ASSERT_FAIL(strand.submit([] -> void { throw 0; }));

    std::vector<int> results;
    std::binary_semaphore sem{0};

    static constexpr auto kNUM_SUBMISSIONS = 1024;

    for (auto i = 1; i <= kNUM_SUBMISSIONS; ++i) {
        if (i == kNUM_SUBMISSIONS) {
            strand.submit([&results, &sem, i] {
                results.push_back(i);
                sem.release();
            });
        } else {
            strand.submit([&results, i] { results.push_back(i); });
        }
    }

    sem.acquire();

    ASSERT_EQ(results.size(), static_cast<size_t>(kNUM_SUBMISSIONS));
    for (int i = 0; i < kNUM_SUBMISSIONS; ++i) {
        EXPECT_EQ(results[i], i + 1);
    }
}

TYPED_TEST(AsyncExecutionContextTests, executeAutoAborts)
{
    auto value = 0;
    std::binary_semaphore sem{0};

    {
        auto res = this->ctx.execute([&](auto stopRequested) {
            while (not stopRequested)
                ;
            value = 42;
            sem.release();
        });
    }  // res goes out of scope and aborts operation

    sem.acquire();
    EXPECT_EQ(value, 42);
}

TYPED_TEST(AsyncExecutionContextTests, repeatingOperationAutoAborts)
{
    auto const repeatDelay = std::chrono::milliseconds{1};
    auto const timeout = std::chrono::milliseconds{15};
    std::atomic_size_t callCount = 0uz;
    auto timeSpentMs = 0u;

    {
        auto res = this->ctx.executeRepeatedly(repeatDelay, [&] { ++callCount; });
        timeSpentMs = util::timed([timeout] {
            std::this_thread::sleep_for(timeout);
        });  // calculate actual time spent
    }  // res goes out of scope and automatically aborts the repeating operation

    // double the delay so that if abort did not happen we will fail below expectations
    std::this_thread::sleep_for(timeout);

    auto const expectedPureCalls = timeout.count() / repeatDelay.count();
    auto const expectedActualCount = timeSpentMs / repeatDelay.count();

    EXPECT_GE(callCount, expectedPureCalls / 2u);  // expect at least half of the scheduled calls
    EXPECT_LE(
        callCount, expectedActualCount + 1u
    );  // at most 1 extra call from an in-flight post that raced with abort
}

using NoErrorHandlerSyncExecutionContext = BasicExecutionContext<
    impl::SameThreadContext,
    impl::BasicStopSource,
    impl::SyncDispatchStrategy,
    impl::SelfContextProvider,
    impl::NoErrorHandler>;

TEST(NoErrorHandlerSyncExecutionContextTests, executeStdException)
{
    auto ctx = NoErrorHandlerSyncExecutionContext{};
    EXPECT_THROW(ctx.execute([] { throw std::runtime_error("test"); }).wait(), std::runtime_error);
}

TEST(NoErrorHandlerSyncExecutionContextTests, executeUnknownException)
{
    auto ctx = NoErrorHandlerSyncExecutionContext{};
    EXPECT_ANY_THROW(ctx.execute([] { throw 0; }).wait());
}

TEST(NoErrorHandlerSyncExecutionContextTests, executeStdExceptionInStrand)
{
    auto ctx = NoErrorHandlerSyncExecutionContext{};
    auto strand = ctx.makeStrand();
    EXPECT_THROW(
        strand.execute([] { throw std::runtime_error("test"); }).wait(), std::runtime_error
    );
}

TEST(NoErrorHandlerSyncExecutionContextTests, executeUnknownExceptionInStrand)
{
    auto ctx = NoErrorHandlerSyncExecutionContext{};
    auto strand = ctx.makeStrand();
    EXPECT_ANY_THROW(strand.execute([] { throw 0; }).wait());
}
