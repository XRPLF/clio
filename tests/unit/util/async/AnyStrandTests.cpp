#include "util/MockOperation.hpp"
#include "util/MockStrand.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/AnyStopToken.hpp"
#include "util/async/AnyStrand.hpp"
#include "util/async/impl/Any.hpp"

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

    template <typename T>
    using RepeatingOperationType = NiceMock<MockRepeatingOperation<T>>;

    ::testing::NaggyMock<MockStrand> mockStrand;
    AnyStrand strand{static_cast<MockStrand&>(mockStrand)};
};

TEST_F(AnyStrandTests, Move)
{
    auto mockOp = OperationType<impl::Any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any()>>())).WillOnce(ReturnRef(mockOp));
    EXPECT_CALL(mockOp, get());

    auto mineNow = std::move(strand);
    ASSERT_TRUE(mineNow.execute([] { throw 0; }).get());
}

TEST_F(AnyStrandTests, CopyIsRefCounted)
{
    auto mockOp = OperationType<impl::Any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any()>>())).WillOnce(ReturnRef(mockOp));

    auto yoink = strand;
    ASSERT_TRUE(yoink.execute([] { throw 0; }).get());
}

TEST_F(AnyStrandTests, ExecuteWithoutTokenAndVoid)
{
    auto mockOp = OperationType<impl::Any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any()>>())).WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([] {});
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyStrandTests, ExecuteWithoutTokenAndVoidThrowsException)
{
    auto mockOp = OperationType<impl::Any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any()>>()))
        .WillOnce([](auto&&) -> OperationType<impl::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([] {}));
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndVoid)
{
    auto mockOp = StoppableOperationType<impl::Any>{};
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([](auto) {});
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndVoidThrowsException)
{
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<impl::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([](auto) {}));
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<impl::Any>{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([](auto) { return 42; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    ASSERT_EQ(op.get().value(), 42);
}

TEST_F(AnyStrandTests, ExecuteWithStopTokenAndReturnValueThrowsException)
{
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<impl::Any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([](auto) { return 42; }));
}

TEST_F(AnyStrandTests, ExecuteWithTimeoutAndStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<impl::Any>{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce(ReturnRef(mockOp));

    auto op = strand.execute([](auto) { return 42; }, std::chrono::milliseconds{1});
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    ASSERT_EQ(op.get().value(), 42);
}

TEST_F(AnyStrandTests, ExecuteWithTimeoutAndStopTokenAndReturnValueThrowsException)
{
    EXPECT_CALL(mockStrand, execute(An<std::function<impl::Any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<impl::Any> const& { throw 0; });

    EXPECT_ANY_THROW(
        [[maybe_unused]] auto unused =
            strand.execute([](auto) { return 42; }, std::chrono::milliseconds{1})
    );
}

TEST_F(AnyStrandTests, RepeatingOperation)
{
    auto mockRepeatingOp = RepeatingOperationType<impl::Any>{};
    EXPECT_CALL(mockRepeatingOp, wait());
    EXPECT_CALL(
        mockStrand, executeRepeatedly(std::chrono::milliseconds{1}, A<std::function<impl::Any()>>())
    )
        .WillOnce([&mockRepeatingOp] -> RepeatingOperationType<impl::Any> const& {
            return mockRepeatingOp;
        });

    auto res = strand.executeRepeatedly(std::chrono::milliseconds{1}, [] -> void { throw 0; });
    static_assert(std::is_same_v<decltype(res), AnyOperation<void>>);
    res.wait();
}
