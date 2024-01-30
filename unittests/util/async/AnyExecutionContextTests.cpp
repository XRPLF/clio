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

#include "util/Expected.h"
#include "util/async/AnyExecutionContext.h"
#include "util/async/AnyOperation.h"
#include "util/async/AnyStopToken.h"
#include "util/async/AnyStrand.h"
#include "util/async/Error.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

using namespace util::async;
using namespace ::testing;

namespace {
struct MockStopSource {
    MOCK_METHOD(void, requestStop, (), ());
};

struct MockStopToken {
    MOCK_METHOD(bool, isStopRequested, (), (const));
};

template <typename ValueType>
struct MockOperation {
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
};

template <typename ValueType>
struct MockStoppableOperation {
    MOCK_METHOD(void, requestStop, (), (const));
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
};

template <typename ValueType>
struct MockScheduledOperation {
    MOCK_METHOD(void, cancel, (), (const));
    MOCK_METHOD(void, requestStop, (), (const));
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
    MOCK_METHOD(void, getToken, (), (const));
};

struct MockStrand {
    template <typename T>
    using ValueType = util::Expected<T, ExecutionError>;

    template <typename T>
    using Operation = MockOperation<T>;

    template <typename T>
    using StoppableOperation = MockStoppableOperation<T>;

    MOCK_METHOD(Operation<detail::Any> const&, execute, (std::function<detail::Any()>), (const));
    MOCK_METHOD(
        Operation<detail::Any> const&,
        execute,
        (std::function<detail::Any()>, std::optional<std::chrono::milliseconds>),
        (const)
    );
    MOCK_METHOD(StoppableOperation<detail::Any> const&, execute, (std::function<detail::Any(AnyStopToken)>), (const));
    MOCK_METHOD(
        StoppableOperation<detail::Any> const&,
        execute,
        (std::function<detail::Any(AnyStopToken)>, std::optional<std::chrono::milliseconds>),
        (const)
    );
};
}  // namespace

struct MockExecutionContext {
    template <typename T>
    using ValueType = util::Expected<T, ExecutionError>;

    using StopSource = MockStopSource;
    using StopToken = MockStopToken;
    using Strand = MockStrand;

    template <typename T>
    using Operation = MockOperation<T>;

    template <typename T>
    using StoppableOperation = MockStoppableOperation<T>;

    template <typename T>
    using ScheduledOperation = MockScheduledOperation<T>;

    MOCK_METHOD(Operation<detail::Any> const&, execute, (std::function<detail::Any()>), (const));
    MOCK_METHOD(
        Operation<detail::Any> const&,
        execute,
        (std::function<detail::Any()>, std::optional<std::chrono::milliseconds>),
        (const)
    );
    MOCK_METHOD(
        StoppableOperation<detail::Any> const&,
        execute,
        (std::function<detail::Any(AnyStopToken)>, std::optional<std::chrono::milliseconds>),
        (const)
    );
    MOCK_METHOD(
        ScheduledOperation<detail::Any> const&,
        scheduleAfter,
        (std::chrono::milliseconds, std::function<detail::Any(AnyStopToken)>),
        (const)
    );
    MOCK_METHOD(
        ScheduledOperation<detail::Any> const&,
        scheduleAfter,
        (std::chrono::milliseconds, std::function<detail::Any(AnyStopToken, bool)>),
        (const)
    );
    MOCK_METHOD(MockStrand const&, makeStrand, (), (const));
    MOCK_METHOD(void, stop, (), (const));
};

struct AnyExecutionContextTests : ::testing::Test {
    using ExecutionContextType = ::testing::NiceMock<MockExecutionContext>;
    using StrandType = ::testing::NiceMock<MockStrand>;

    template <typename T>
    using OperationType = ::testing::NiceMock<MockOperation<T>>;

    template <typename T>
    using StoppableOperationType = ::testing::NiceMock<MockStoppableOperation<T>>;

    template <typename T>
    using ScheduledOperationType = ::testing::NiceMock<MockScheduledOperation<T>>;

    ::testing::NaggyMock<MockExecutionContext> mockExecutionContext;
    AnyExecutionContext ctx{static_cast<MockExecutionContext&>(mockExecutionContext)};
};

TEST_F(AnyExecutionContextTests, ExecuteWithoutTokenAndVoid)
{
    auto mockOp = OperationType<detail::Any>{};
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<detail::Any()>>())).WillOnce(ReturnRef(mockOp));

    auto op = ctx.execute([] {});
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, ExecuteWithoutTokenAndVoidThrowsException)
{
    auto mockOp = OperationType<detail::Any>{};
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<detail::Any()>>()))
        .WillOnce([](auto&&) -> OperationType<detail::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto _ = ctx.execute([] {}));
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndVoid)
{
    auto mockOp = StoppableOperationType<detail::Any>{};
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<detail::Any(AnyStopToken)>>(), _))
        .WillOnce(ReturnRef(mockOp));

    auto op = ctx.execute([](auto) {});
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndVoidThrowsException)
{
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<detail::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<detail::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto _ = ctx.execute([](auto) {}));
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<detail::Any>{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<detail::Any(AnyStopToken)>>(), _))
        .WillOnce(ReturnRef(mockOp));

    auto op = ctx.execute([](auto) { return 42; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    ASSERT_EQ(op.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndReturnValueThrowsException)
{
    EXPECT_CALL(mockExecutionContext, execute(An<std::function<detail::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<detail::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto _ = ctx.execute([](auto) { return 42; }));
}

TEST_F(AnyExecutionContextTests, TimerCancellation)
{
    auto mockScheduledOp = ScheduledOperationType<detail::Any>{};
    EXPECT_CALL(mockScheduledOp, cancel()).Times(1);
    EXPECT_CALL(
        mockExecutionContext,
        scheduleAfter(An<std::chrono::milliseconds>(), An<std::function<detail::Any(AnyStopToken)>>())
    )
        .WillOnce(ReturnRef(mockScheduledOp));

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{42}, [](auto) {});
    static_assert(std::is_same_v<decltype(timer), AnyOperation<void>>);

    timer.cancel();
}

TEST_F(AnyExecutionContextTests, TimerExecuted)
{
    auto mockScheduledOp = ScheduledOperationType<detail::Any>{};
    EXPECT_CALL(mockScheduledOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(
        mockExecutionContext,
        scheduleAfter(An<std::chrono::milliseconds>(), An<std::function<detail::Any(AnyStopToken)>>())
    )
        .WillOnce([&mockScheduledOp](auto, auto&&) -> ScheduledOperationType<detail::Any> const& {
            return mockScheduledOp;
        });

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{42}, [](auto) { return 42; });

    static_assert(std::is_same_v<decltype(timer), AnyOperation<int>>);
    EXPECT_EQ(timer.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, TimerWithBoolHandlerCancellation)
{
    auto mockScheduledOp = ScheduledOperationType<detail::Any>{};
    EXPECT_CALL(mockScheduledOp, cancel()).Times(1);
    EXPECT_CALL(
        mockExecutionContext,
        scheduleAfter(An<std::chrono::milliseconds>(), An<std::function<detail::Any(AnyStopToken, bool)>>())
    )
        .WillOnce(ReturnRef(mockScheduledOp));

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{42}, [](auto, bool) {});
    static_assert(std::is_same_v<decltype(timer), AnyOperation<void>>);

    timer.cancel();
}

TEST_F(AnyExecutionContextTests, TimerWithBoolHandlerExecuted)
{
    auto mockScheduledOp = ScheduledOperationType<detail::Any>{};
    EXPECT_CALL(mockScheduledOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(
        mockExecutionContext,
        scheduleAfter(An<std::chrono::milliseconds>(), An<std::function<detail::Any(AnyStopToken, bool)>>())
    )
        .WillOnce([&mockScheduledOp](auto, auto&&) -> ScheduledOperationType<detail::Any> const& {
            return mockScheduledOp;
        });

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{42}, [](auto, bool) { return 42; });

    static_assert(std::is_same_v<decltype(timer), AnyOperation<int>>);
    EXPECT_EQ(timer.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithVoid)
{
    auto mockOp = OperationType<detail::Any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any()>>())).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([] {});
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithVoidThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any()>>()))
        .WillOnce([](auto&&) -> OperationType<detail::Any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto _ = strand.execute([] {}));
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithReturnValue)
{
    auto mockOp = OperationType<detail::Any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any()>>())).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([] { return 42; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    EXPECT_EQ(op.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithReturnValueThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any()>>()))
        .WillOnce([](auto&&) -> OperationType<detail::Any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto _ = strand.execute([] { return 42; }));
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndVoid)
{
    auto mockOp = StoppableOperationType<detail::Any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([](auto) {});
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndVoidThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<detail::Any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto _ = strand.execute([](auto) {}));
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<detail::Any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([](auto) { return 42; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    EXPECT_EQ(op.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndReturnValueThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<detail::Any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto _ = strand.execute([](auto) { return 42; }));
}
