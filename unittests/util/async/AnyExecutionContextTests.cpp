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

#include "util/MockExecutionContext.hpp"
#include "util/MockOperation.hpp"
#include "util/MockStrand.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyStopToken.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <functional>

using namespace util::async;
using namespace ::testing;

struct AnyExecutionContextTests : Test {
    using StrandType = NiceMock<MockStrand>;

    template <typename T>
    using OperationType = NiceMock<MockOperation<T>>;

    template <typename T>
    using StoppableOperationType = NiceMock<MockStoppableOperation<T>>;

    template <typename T>
    using ScheduledOperationType = NiceMock<MockScheduledOperation<T>>;

    NiceMock<MockExecutionContext> mockExecutionContext;
    AnyExecutionContext ctx{static_cast<MockExecutionContext&>(mockExecutionContext)};
};

TEST_F(AnyExecutionContextTests, ExecuteWithoutTokenAndVoid)
{
    auto mockOp = OperationType<impl::Any>{};
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<impl::Any()>>())).WillOnce(ReturnRef(mockOp));
    EXPECT_CALL(mockOp, get());

    auto op = ctx.execute([] { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, ExecuteWithoutTokenAndVoidThrowsException)
{
    auto mockOp = OperationType<impl::Any>{};
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<impl::Any()>>()))
        .WillOnce([](auto&&) -> OperationType<impl::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = ctx.execute([] { throw 0; }));
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndVoid)
{
    auto mockOp = StoppableOperationType<impl::Any>{};
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce(ReturnRef(mockOp));
    EXPECT_CALL(mockOp, get());

    auto op = ctx.execute([](auto) { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndVoidThrowsException)
{
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<impl::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = ctx.execute([](auto) { throw 0; }));
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<impl::Any>{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce(ReturnRef(mockOp));

    auto op = ctx.execute([](auto) -> int { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    ASSERT_EQ(op.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndReturnValueThrowsException)
{
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<impl::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = ctx.execute([](auto) -> int { throw 0; }));
}

TEST_F(AnyExecutionContextTests, TimerCancellation)
{
    auto mockScheduledOp = ScheduledOperationType<impl::Any>{};
    EXPECT_CALL(mockScheduledOp, cancel());
    EXPECT_CALL(
        mockExecutionContext,
        scheduleAfter(An<std::chrono::milliseconds>(), An<std::function<impl::Any(AnyStopToken)>>())
    )
        .WillOnce(ReturnRef(mockScheduledOp));

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{12}, [](auto) { throw 0; });
    static_assert(std::is_same_v<decltype(timer), AnyOperation<void>>);

    timer.cancel();
}

TEST_F(AnyExecutionContextTests, TimerExecuted)
{
    auto mockScheduledOp = ScheduledOperationType<impl::Any>{};
    EXPECT_CALL(mockScheduledOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(
        mockExecutionContext,
        scheduleAfter(An<std::chrono::milliseconds>(), An<std::function<impl::Any(AnyStopToken)>>())
    )
        .WillOnce([&mockScheduledOp](auto, auto&&) -> ScheduledOperationType<impl::Any> const& {
            return mockScheduledOp;
        });

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{12}, [](auto) -> int { throw 0; });

    static_assert(std::is_same_v<decltype(timer), AnyOperation<int>>);
    EXPECT_EQ(timer.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, TimerWithBoolHandlerCancellation)
{
    auto mockScheduledOp = ScheduledOperationType<impl::Any>{};
    EXPECT_CALL(mockScheduledOp, cancel());
    EXPECT_CALL(
        mockExecutionContext,
        scheduleAfter(An<std::chrono::milliseconds>(), An<std::function<impl::Any(AnyStopToken, bool)>>())
    )
        .WillOnce(ReturnRef(mockScheduledOp));

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{12}, [](auto, bool) { throw 0; });
    static_assert(std::is_same_v<decltype(timer), AnyOperation<void>>);

    timer.cancel();
}

TEST_F(AnyExecutionContextTests, TimerWithBoolHandlerExecuted)
{
    auto mockScheduledOp = ScheduledOperationType<impl::Any>{};
    EXPECT_CALL(mockScheduledOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(
        mockExecutionContext,
        scheduleAfter(An<std::chrono::milliseconds>(), An<std::function<impl::Any(AnyStopToken, bool)>>())
    )
        .WillOnce([&mockScheduledOp](auto, auto&&) -> ScheduledOperationType<impl::Any> const& {
            return mockScheduledOp;
        });

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{12}, [](auto, bool) -> int { throw 0; });

    static_assert(std::is_same_v<decltype(timer), AnyOperation<int>>);
    EXPECT_EQ(timer.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithVoid)
{
    auto mockOp = OperationType<impl::Any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockOp, get());
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any()>>())).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([] { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithVoidThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any()>>()))
        .WillOnce([](auto&&) -> OperationType<impl::Any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([] {}));
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithReturnValue)
{
    auto mockOp = OperationType<impl::Any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any()>>())).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([]() -> int { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    EXPECT_EQ(op.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithReturnValueThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any()>>()))
        .WillOnce([](auto&&) -> OperationType<impl::Any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([]() -> int { throw 0; }));
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndVoid)
{
    auto mockOp = StoppableOperationType<impl::Any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockOp, get());
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([](auto) { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndVoidThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<impl::Any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([](auto) { throw 0; }));
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<impl::Any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([](auto) -> int { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    EXPECT_EQ(op.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndReturnValueThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<impl::Any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([](auto) -> int { throw 0; }));
}
