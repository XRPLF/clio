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
#include "util/MockOperation.h"
#include "util/MockStrand.h"
#include "util/async/AnyOperation.h"
#include "util/async/AnyStopToken.h"
#include "util/async/AnyStrand.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <chrono>
#include <functional>
#include <type_traits>

using namespace util::async;
using namespace ::testing;

struct AnyStrandTests : ::testing::Test {
    template <typename T>
    using OperationType = ::testing::NiceMock<MockOperation<T>>;

    template <typename T>
    using StoppableOperationType = ::testing::NiceMock<MockStoppableOperation<T>>;

    ::testing::NaggyMock<MockStrand> mockStrand;
    AnyStrand strand{static_cast<MockStrand&>(mockStrand)};
};

TEST_F(AnyStrandTests, ExecuteWithoutTokenAndVoid)
{
    auto mockOp = OperationType<detail::Any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any()>>())).WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([] {});
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyStrandTests, ExecuteWithoutTokenAndVoidThrowsException)
{
    auto mockOp = OperationType<detail::Any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any()>>()))
        .WillOnce([](auto&&) -> OperationType<detail::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([] {}));
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndVoid)
{
    auto mockOp = StoppableOperationType<detail::Any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([](auto) {});
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndVoidThrowsException)
{
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<detail::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([](auto) {}));
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<detail::Any>{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([](auto) { return 42; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    ASSERT_EQ(op.get().value(), 42);
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndReturnValueThrowsException)
{
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<detail::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([](auto) { return 42; }));
}

TEST_F(AnyStrandTests, ExecuteWithTimeoutAndStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<detail::Any>{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([](auto) { return 42; }, std::chrono::milliseconds{1});
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    ASSERT_EQ(op.get().value(), 42);
}

TEST_F(AnyStrandTests, ExecuteWithTimoutAndStopTokenAndReturnValueThrowsException)
{
    EXPECT_CALL(mockStrand, execute(An<std::function<detail::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<detail::Any> const& { throw 0; });

    EXPECT_ANY_THROW(
        [[maybe_unused]] auto unused = strand.execute([](auto) { return 42; }, std::chrono::milliseconds{1})
    );
}
