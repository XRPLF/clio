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
}  // namespace

struct AnyOperationTests : ::testing::Test {
    using OperationType = MockOperation<util::Expected<detail::Any, ExecutionError>>;
    using ScheduledOperationType = MockScheduledOperation<util::Expected<detail::Any, ExecutionError>>;

    ::testing::NaggyMock<OperationType> mockOp;
    ::testing::NaggyMock<ScheduledOperationType> mockScheduledOp;

    AnyOperation<void> voidOp{detail::ErasedOperation(static_cast<OperationType&>(mockOp))};
    AnyOperation<int> intOp{detail::ErasedOperation(static_cast<OperationType&>(mockOp))};
    AnyOperation<void> scheduledVoidOp{detail::ErasedOperation(static_cast<ScheduledOperationType&>(mockScheduledOp))};
};

TEST_F(AnyOperationTests, VoidDataYieldsNoError)
{
    auto const noError = util::Expected<detail::Any, ExecutionError>(detail::Any{});
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
    auto called = false;
    EXPECT_CALL(mockOp, wait()).WillOnce([&] { called = true; });
    ;
    voidOp.wait();
    EXPECT_TRUE(called);
}

TEST_F(AnyOperationTests, CancelCallPropagated)
{
    auto called = false;
    EXPECT_CALL(mockScheduledOp, cancel()).WillOnce([&] { called = true; });
    ;
    scheduledVoidOp.cancel();
    EXPECT_TRUE(called);
}

TEST_F(AnyOperationTests, RequestStopCallPropagated)
{
    auto called = false;
    EXPECT_CALL(mockScheduledOp, requestStop()).WillOnce([&] { called = true; });
    scheduledVoidOp.requestStop();
    EXPECT_TRUE(called);
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
