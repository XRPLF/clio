#include "cluster/impl/FallbackRecoveryTimer.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/thread_pool.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <semaphore>

using namespace cluster::impl;
using namespace testing;

struct FallbackRecoveryTimerTest : Test {
    ~FallbackRecoveryTimerTest() override
    {
        ctx_.join();
    }

protected:
    boost::asio::thread_pool ctx_{1};
};

TEST_F(FallbackRecoveryTimerTest, NotRunningByDefault)
{
    FallbackRecoveryTimer const timer{ctx_, std::chrono::hours{1}};
    EXPECT_FALSE(timer.isRunning());
}

TEST_F(FallbackRecoveryTimerTest, IsRunningAfterStart)
{
    FallbackRecoveryTimer timer{ctx_, std::chrono::hours{1}};
    timer.start([](boost::system::error_code) {});
    EXPECT_TRUE(timer.isRunning());
    timer.cancel();
}

TEST_F(FallbackRecoveryTimerTest, NotRunningAfterCancel)
{
    FallbackRecoveryTimer timer{ctx_, std::chrono::hours{1}};
    timer.start([](boost::system::error_code) {});
    timer.cancel();
    EXPECT_FALSE(timer.isRunning());
}

TEST_F(FallbackRecoveryTimerTest, CallbackFiresWithNoError)
{
    std::binary_semaphore sem{0};
    std::atomic<bool> callbackFired{false};
    boost::system::error_code capturedEc{};

    FallbackRecoveryTimer timer{ctx_, std::chrono::milliseconds{0}};
    timer.start([&](boost::system::error_code ec) {
        capturedEc = ec;
        callbackFired = true;
        sem.release();
    });

    EXPECT_TRUE(sem.try_acquire_for(std::chrono::seconds{5}));
    EXPECT_TRUE(callbackFired);
    EXPECT_FALSE(capturedEc);
}

TEST_F(FallbackRecoveryTimerTest, NotRunningAfterCallbackFires)
{
    std::binary_semaphore sem{0};

    FallbackRecoveryTimer timer{ctx_, std::chrono::milliseconds{0}};
    timer.start([&](boost::system::error_code) { sem.release(); });

    EXPECT_TRUE(sem.try_acquire_for(std::chrono::seconds{5}));
    // Give the callback a moment to clear isRunning_
    ctx_.join();
    EXPECT_FALSE(timer.isRunning());
}

TEST_F(FallbackRecoveryTimerTest, CallbackReceivesOperationAbortedOnCancel)
{
    std::binary_semaphore sem{0};
    boost::system::error_code capturedEc{};

    FallbackRecoveryTimer timer{ctx_, std::chrono::hours{1}};
    timer.start([&](boost::system::error_code ec) {
        capturedEc = ec;
        sem.release();
    });

    timer.cancel();

    EXPECT_TRUE(sem.try_acquire_for(std::chrono::seconds{5}));
    EXPECT_EQ(capturedEc, boost::asio::error::operation_aborted);
}

TEST_F(FallbackRecoveryTimerTest, CancelOnNonRunningTimerIsNoOp)
{
    FallbackRecoveryTimer timer{ctx_, std::chrono::hours{1}};
    EXPECT_FALSE(timer.isRunning());
    EXPECT_NO_FATAL_FAILURE({ timer.cancel(); });
    EXPECT_FALSE(timer.isRunning());
}

TEST_F(FallbackRecoveryTimerTest, SharedPtrUsageWorks)
{
    std::binary_semaphore sem{0};
    std::atomic<bool> callbackFired{false};

    auto sharedTimer = std::make_shared<FallbackRecoveryTimer>(ctx_, std::chrono::milliseconds{0});

    EXPECT_FALSE(sharedTimer->isRunning());

    sharedTimer->start([&](boost::system::error_code ec) {
        if (ec == boost::asio::error::operation_aborted)
            return;
        callbackFired = true;
        sem.release();
    });

    EXPECT_TRUE(sharedTimer->isRunning());
    EXPECT_TRUE(sem.try_acquire_for(std::chrono::seconds{5}));
    EXPECT_TRUE(callbackFired);
}
