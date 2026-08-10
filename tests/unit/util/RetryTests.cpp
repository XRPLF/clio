#include "util/AsioContextTestFixture.hpp"
#include "util/Retry.hpp"

#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>

using namespace util;

struct RetryTests : virtual ::testing::Test {
protected:
    std::chrono::milliseconds const delay_{1};
    std::chrono::milliseconds const maxDelay_{5};
};

TEST_F(RetryTests, ExponentialBackoffStrategy)
{
    ExponentialBackoffStrategy strategy{{.initial = delay_, .max = maxDelay_}};

    EXPECT_EQ(strategy.getDelay(), delay_);

    strategy.increaseDelay();
    EXPECT_EQ(strategy.getDelay(), delay_ * 2);

    strategy.increaseDelay();
    EXPECT_LT(strategy.getDelay(), maxDelay_);

    for (size_t i = 0; i < 10; ++i) {
        strategy.increaseDelay();
        EXPECT_EQ(strategy.getDelay(), maxDelay_);
        EXPECT_EQ(strategy.getDelay(), maxDelay_);
    }

    strategy.reset();
    EXPECT_EQ(strategy.getDelay(), delay_);
}

struct RetryWithExponentialBackoffStrategyTests : SyncAsioContextTest, RetryTests {
    RetryWithExponentialBackoffStrategyTests()
    {
        EXPECT_EQ(retry_.attemptNumber(), 0);
        EXPECT_EQ(retry_.delayValue(), delay_);
    }

protected:
    Retry retry_ = makeRetryExponentialBackoff(
        {.initial = delay_, .max = maxDelay_},
        boost::asio::make_strand(ctx_)
    );
    testing::MockFunction<void()> mockCallback_;
};

TEST_F(RetryWithExponentialBackoffStrategyTests, Retry)
{
    retry_.retry(mockCallback_.AsStdFunction());

    EXPECT_EQ(retry_.attemptNumber(), 0);

    EXPECT_CALL(mockCallback_, Call());
    runContext();

    EXPECT_EQ(retry_.attemptNumber(), 1);
    EXPECT_EQ(retry_.delayValue(), delay_ * 2);
}

TEST_F(RetryWithExponentialBackoffStrategyTests, Cancel)
{
    retry_.retry(mockCallback_.AsStdFunction());
    retry_.cancel();
    runContext();
    EXPECT_EQ(retry_.attemptNumber(), 0);

    retry_.cancel();
    EXPECT_EQ(retry_.attemptNumber(), 0);
}

TEST_F(RetryWithExponentialBackoffStrategyTests, Reset)
{
    retry_.retry(mockCallback_.AsStdFunction());

    EXPECT_CALL(mockCallback_, Call());
    runContext();

    EXPECT_EQ(retry_.attemptNumber(), 1);
    EXPECT_EQ(retry_.delayValue(), delay_ * 2);

    retry_.reset();
    EXPECT_EQ(retry_.attemptNumber(), 0);
    EXPECT_EQ(retry_.delayValue(), delay_);
}

struct RetryWaitTests : SyncAsioContextTest, RetryTests {};

TEST_F(RetryWaitTests, WaitOnCoroutineAdvancesAttemptAndDelay)
{
    runSpawn([this](auto yield) {
        auto retry = makeRetryExponentialBackoff(
            {.initial = delay_, .max = maxDelay_}, yield.get_executor()
        );

        EXPECT_EQ(retry.attemptNumber(), 0);
        EXPECT_EQ(retry.delayValue(), delay_);

        retry.wait(yield);
        EXPECT_EQ(retry.attemptNumber(), 1);
        EXPECT_EQ(retry.delayValue(), delay_ * 2);

        retry.wait(yield);
        EXPECT_EQ(retry.attemptNumber(), 2);
    });
}

TEST_F(RetryWaitTests, WaitDoesNotBlockItsThread)
{
    bool ran = false;
    bool ranBeforeWaitReturned = false;

    runSpawn([this, &ran, &ranBeforeWaitReturned](auto yield) {
        boost::asio::post(ctx_, [&ran]() { ran = true; });

        auto retry = makeRetryExponentialBackoff(
            {.initial = std::chrono::milliseconds{20}, .max = std::chrono::milliseconds{20}},
            yield.get_executor()
        );
        retry.wait(yield);

        ranBeforeWaitReturned = ran;
    });

    EXPECT_TRUE(ranBeforeWaitReturned);
}
