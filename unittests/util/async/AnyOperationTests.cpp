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

#include "util/Expected.hpp"
#include "util/MockOperation.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/Error.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <string>

using namespace util::async;
using namespace ::testing;

struct AnyOperationTests : Test {
    using OperationType = MockOperation<util::Expected<impl::Any, ExecutionError>>;
    using ScheduledOperationType = MockScheduledOperation<util::Expected<impl::Any, ExecutionError>>;

    NaggyMock<OperationType> mockOp;
    NaggyMock<ScheduledOperationType> mockScheduledOp;

    AnyOperation<void> voidOp{impl::ErasedOperation(static_cast<OperationType&>(mockOp))};
    AnyOperation<int> intOp{impl::ErasedOperation(static_cast<OperationType&>(mockOp))};
    AnyOperation<void> scheduledVoidOp{impl::ErasedOperation(static_cast<ScheduledOperationType&>(mockScheduledOp))};
};

TEST_F(AnyOperationTests, VoidDataYieldsNoError)
{
    auto const noError = util::Expected<impl::Any, ExecutionError>(impl::Any{});
    EXPECT_CALL(mockOp, get()).WillOnce(Return(noError));
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

TEST_F(AnyOperationTests, CancelCallPropagated)
{
    StrictMock<MockFunction<void()>> callback;
    EXPECT_CALL(callback, Call());
    EXPECT_CALL(mockScheduledOp, cancel()).WillOnce([&] { callback.Call(); });
    scheduledVoidOp.cancel();
}

TEST_F(AnyOperationTests, RequestStopCallPropagated)
{
    StrictMock<MockFunction<void()>> callback;
    EXPECT_CALL(callback, Call());
    EXPECT_CALL(mockScheduledOp, requestStop()).WillOnce([&] { callback.Call(); });
    scheduledVoidOp.requestStop();
}

TEST_F(AnyOperationTests, GetPropagatesError)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(util::Unexpected(ExecutionError{"tid", "Not good"})));
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
