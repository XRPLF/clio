#include "util/MockAssert.hpp"
#include "util/async/AnyStopToken.hpp"

#include <boost/asio/spawn.hpp>
#include <gtest/gtest.h>

using namespace util::async;
using namespace ::testing;

namespace {
struct FakeStopToken {
    bool stopRequested = false;

    [[nodiscard]] bool
    isStopRequested() const
    {
        return stopRequested;
    }
};
}  // namespace

struct AnyStopTokenTests : public TestWithParam<bool> {};

INSTANTIATE_TEST_CASE_P(
    AnyStopTokenGroup,
    AnyStopTokenTests,
    ValuesIn({true, false}),
    [](auto const& info) { return info.param ? "true" : "false"; }
);

TEST_P(AnyStopTokenTests, CanCopy)
{
    AnyStopToken const stopToken{FakeStopToken{GetParam()}};
    AnyStopToken const token = stopToken;

    EXPECT_EQ(token, stopToken);
}

TEST_P(AnyStopTokenTests, IsStopRequestedCallPropagated)
{
    auto const flag = GetParam();
    AnyStopToken const stopToken{FakeStopToken{flag}};

    EXPECT_EQ(stopToken.isStopRequested(), flag);
    EXPECT_EQ(stopToken, flag);
}

struct AnyStopTokenAssertTest : common::util::WithMockAssert {};

TEST_F(AnyStopTokenAssertTest, ConversionToYieldContextAssertsIfUnsupported)
{
    EXPECT_CLIO_ASSERT_FAIL(
        [[maybe_unused]] auto unused =
            static_cast<boost::asio::yield_context>(AnyStopToken{FakeStopToken{}})
    );
}
