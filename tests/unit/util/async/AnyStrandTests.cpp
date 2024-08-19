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

#include "util/MockOperation.hpp"
#include "util/MockStrand.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/AnyStopToken.hpp"
#include "util/async/AnyStrand.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <chrono>
#include <expected>
#include <functional>
#include <type_traits>
#include <utility>

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

TEST_F(AnyStrandTests, Move)
{
    auto mockOp = OperationType<std::any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<std::any()>>())).WillOnce(ReturnRef(mockOp));
    EXPECT_CALL(mockOp, get());

    auto mineNow = std::move(strand);
    ASSERT_TRUE(mineNow.execute([] { throw 0; }).get());
}

TEST_F(AnyStrandTests, CopyIsRefCounted)
{
    auto mockOp = OperationType<std::any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<std::any()>>())).WillOnce(ReturnRef(mockOp));

    auto yoink = strand;
    ASSERT_TRUE(yoink.execute([] { throw 0; }).get());
}

TEST_F(AnyStrandTests, ExecuteWithoutTokenAndVoid)
{
    auto mockOp = OperationType<std::any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<std::any()>>())).WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([] {});
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyStrandTests, ExecuteWithoutTokenAndVoidThrowsException)
{
    auto mockOp = OperationType<std::any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<std::any()>>()))
        .WillOnce([](auto&&) -> OperationType<std::any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([] {}));
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndVoid)
{
    auto mockOp = StoppableOperationType<std::any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<std::any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([](auto) {});
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndVoidThrowsException)
{
    EXPECT_CALL(mockStrand, execute(An<std::function<std::any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<std::any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([](auto) {}));
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<std::any>{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockStrand, execute(An<std::function<std::any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([](auto) { return 42; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    ASSERT_EQ(op.get().value(), 42);
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndReturnValueThrowsException)
{
    EXPECT_CALL(mockStrand, execute(An<std::function<std::any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<std::any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([](auto) { return 42; }));
}

TEST_F(AnyStrandTests, ExecuteWithTimeoutAndStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<std::any>{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockStrand, execute(An<std::function<std::any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([](auto) { return 42; }, std::chrono::milliseconds{1});
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    ASSERT_EQ(op.get().value(), 42);
}

TEST_F(AnyStrandTests, ExecuteWithTimoutAndStopTokenAndReturnValueThrowsException)
{
    EXPECT_CALL(mockStrand, execute(An<std::function<std::any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<std::any> const& { throw 0; });

    EXPECT_ANY_THROW(
        [[maybe_unused]] auto unused = strand.execute([](auto) { return 42; }, std::chrono::milliseconds{1})
    );
}
