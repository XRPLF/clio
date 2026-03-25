#include "util/ScopeGuard.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(ScopeGuardTest, IsCalled)
{
    testing::StrictMock<testing::MockFunction<void()>> mockFunction;
    EXPECT_CALL(mockFunction, Call());
    {
        util::ScopeGuard const guard([&mockFunction] { mockFunction.Call(); });
    }
}
