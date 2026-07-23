#include "util/MockAssert.hpp"
#include "util/MockOperation.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/Error.hpp"
#include "util/async/impl/Any.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <expected>
#include <string>
#include <utility>

using namespace util::async;
using namespace ::testing;

struct AnyOperationTests : virtual Test {
    using OperationType = MockOperation<std::expected<impl::Any, ExecutionError>>;
    using StoppableOperationType = MockStoppableOperation<std::expected<impl::Any, ExecutionError>>;
    using ScheduledOperationType = MockScheduledOperation<std::expected<impl::Any, ExecutionError>>;
    using RepeatingOperationType = MockRepeatingOperation<std::expected<impl::Any, ExecutionError>>;

    NaggyMock<OperationType> mockOp;
    NaggyMock<StoppableOperationType> mockStoppableOp;
    NaggyMock<ScheduledOperationType> mockScheduledOp;
    NaggyMock<RepeatingOperationType> mockRepeatingOp;

    AnyOperation<void> voidOp{impl::ErasedOperation(static_cast<OperationType&>(mockOp))};
    AnyOperation<void> voidStoppableOp{
        impl::ErasedOperation(static_cast<StoppableOperationType&>(mockStoppableOp))
    };
    AnyOperation<int> intOp{impl::ErasedOperation(static_cast<OperationType&>(mockOp))};
    AnyOperation<void> scheduledVoidOp{
        impl::ErasedOperation(static_cast<ScheduledOperationType&>(mockScheduledOp))
    };
    AnyOperation<void> repeatingOp{
        impl::ErasedOperation(static_cast<RepeatingOperationType&>(mockRepeatingOp))
    };
};

TEST_F(AnyOperationTests, Move)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(impl::Any{}));
    auto yoink = std::move(voidOp);
    auto res = yoink.get();
    ASSERT_TRUE(res);
}

TEST_F(AnyOperationTests, VoidDataYieldsNoError)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(impl::Any{}));
    auto res = voidOp.get();
    ASSERT_TRUE(res);
}

TEST_F(AnyOperationTests, GetIntData)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    auto res = intOp.get();
    EXPECT_EQ(res.value(), 42);
}

TEST_F(AnyOperationTests, WaitCallPropagated)
{
    StrictMock<MockFunction<void()>> callback;
    EXPECT_CALL(callback, Call());
    EXPECT_CALL(mockOp, wait()).WillOnce([&] { callback.Call(); });
    voidOp.wait();
}

TEST_F(AnyOperationTests, CancelAndRequestStopCallPropagated)
{
    StrictMock<MockFunction<void()>> callback;
    EXPECT_CALL(callback, Call()).Times(2);
    EXPECT_CALL(mockScheduledOp, cancel()).WillOnce([&] { callback.Call(); });
    EXPECT_CALL(mockScheduledOp, requestStop()).WillOnce([&] { callback.Call(); });
    scheduledVoidOp.abort();
}

TEST_F(AnyOperationTests, RequestStopCallPropagatedOnStoppableOperation)
{
    StrictMock<MockFunction<void()>> callback;
    EXPECT_CALL(callback, Call());
    EXPECT_CALL(mockStoppableOp, requestStop()).WillOnce([&] { callback.Call(); });
    voidStoppableOp.abort();
}

TEST_F(AnyOperationTests, GetPropagatesError)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::unexpected(ExecutionError{"tid", "Not good"})));
    auto res = intOp.get();
    ASSERT_FALSE(res);
    EXPECT_TRUE(res.error().message.ends_with("Not good"));
}

TEST_F(AnyOperationTests, GetIncorrectDataReturnsError)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<double>(4.2)));
    auto res = intOp.get();

    ASSERT_FALSE(res);
    EXPECT_TRUE(res.error().message.ends_with("Bad any cast"));
    EXPECT_TRUE(std::string{res.error()}.ends_with("Bad any cast"));
}

TEST_F(AnyOperationTests, RepeatingOpWaitPropagated)
{
    EXPECT_CALL(mockRepeatingOp, wait());
    repeatingOp.wait();
}

TEST_F(AnyOperationTests, RepeatingOpRequestStopCallPropagated)
{
    EXPECT_CALL(mockRepeatingOp, requestStop());
    repeatingOp.abort();
}

TEST_F(AnyOperationTests, RepeatingOpInvokeCallPropagated)
{
    EXPECT_CALL(mockRepeatingOp, invoke());
    repeatingOp.invoke();
}

struct AnyOperationAssertTest : common::util::WithMockAssert, AnyOperationTests {};

TEST_F(AnyOperationAssertTest, CallAbortOnNonStoppableOrCancellableOperation)
{
    EXPECT_CLIO_ASSERT_FAIL(voidOp.abort());
}

TEST_F(AnyOperationAssertTest, CallInvokeOnNonForceInvocableOperation)
{
    EXPECT_CLIO_ASSERT_FAIL(voidOp.invoke());
}
