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

#include "util/async/context/BasicExecutionContext.h"
#include "util/async/context/SyncExecutionContext.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <semaphore>
#include <stdexcept>
#include <string>

using namespace util::async;
using ::testing::Types;

using ExecutionContextTypes = Types<CoroExecutionContext, PoolExecutionContext, SyncExecutionContext>;

template <typename T>
struct ExecutionContextTests : public ::testing::Test {
    using ExecutionContextType = T;
    ExecutionContextType ctx{2};
};

TYPED_TEST_CASE(ExecutionContextTests, ExecutionContextTypes);

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
    auto res =
        this->ctx.scheduleAfter(std::chrono::milliseconds(1), []([[maybe_unused]] auto stopRequested, auto cancelled) {
            if (not cancelled)
                return 42;
            return 0;
        });

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

TYPED_TEST(ExecutionContextTests, timerStdException)
{
    auto res =
        this->ctx.scheduleAfter(std::chrono::milliseconds(1), []([[maybe_unused]] auto stopRequested, auto cancelled) {
            if (not cancelled)
                throw std::runtime_error("test");
            return 0;
        });

    auto const err = res.get().error();
    EXPECT_TRUE(err.message.ends_with("test"));
    EXPECT_TRUE(std::string{err}.ends_with("test"));
}

TYPED_TEST(ExecutionContextTests, timerUnknownException)
{
    auto res =
        this->ctx.scheduleAfter(std::chrono::milliseconds(1), []([[maybe_unused]] auto stopRequested, auto cancelled) {
            if (not cancelled)
                throw 0;
            return 0;
        });

    auto const err = res.get().error();
    EXPECT_TRUE(err.message.ends_with("unknown"));
    EXPECT_TRUE(std::string{err}.ends_with("unknown"));
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

using NoErrorHandlerSyncExecutionContext = BasicExecutionContext<
    detail::SameThreadContext,
    detail::BasicStopSource,
    detail::SyncDispatchStrategy,
    detail::SelfContextProvider,
    detail::NoErrorHandler>;

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
    EXPECT_THROW(strand.execute([] { throw std::runtime_error("test"); }).wait(), std::runtime_error);
}

TEST(NoErrorHandlerSyncExecutionContextTests, executeUnknownExceptionInStrand)
{
    auto ctx = NoErrorHandlerSyncExecutionContext{};
    auto strand = ctx.makeStrand();
    EXPECT_ANY_THROW(strand.execute([] { throw 0; }).wait());
}
