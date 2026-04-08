#include "data/LedgerCache.hpp"
#include "data/LedgerCacheLoadingState.hpp"
#include "util/MockPrometheus.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <semaphore>
#include <thread>

using namespace data;

struct LedgerCacheLoadingStateTest : util::prometheus::WithPrometheus {
    LedgerCache cache;
    LedgerCacheLoadingState state{cache};
};

TEST_F(LedgerCacheLoadingStateTest, DefaultStateLoadingNotAllowed)
{
    EXPECT_FALSE(state.isLoadingAllowed());
}

TEST_F(LedgerCacheLoadingStateTest, AllowLoadingPermitsLoading)
{
    state.allowLoading();
    EXPECT_TRUE(state.isLoadingAllowed());
}

TEST_F(LedgerCacheLoadingStateTest, IsCurrentlyLoadingDelegatesToCache)
{
    EXPECT_FALSE(state.isCurrentlyLoading());
    cache.startLoading();
    EXPECT_TRUE(state.isCurrentlyLoading());
    cache.setFull();
    EXPECT_FALSE(state.isCurrentlyLoading());
}

TEST_F(LedgerCacheLoadingStateTest, CloneSharesAllowedFlag)
{
    auto clone = state.clone();
    ASSERT_NE(clone, nullptr);

    EXPECT_FALSE(clone->isLoadingAllowed());
    state.allowLoading();
    EXPECT_TRUE(clone->isLoadingAllowed());
}

TEST_F(LedgerCacheLoadingStateTest, AllowLoadingOnCloneVisibleToOriginal)
{
    auto clone = state.clone();
    clone->allowLoading();
    EXPECT_TRUE(state.isLoadingAllowed());
}

TEST_F(LedgerCacheLoadingStateTest, WaitForLoadingAllowedReturnsIfAlreadyAllowed)
{
    state.allowLoading();
    std::binary_semaphore done{0};
    std::thread t{[&] {
        state.waitForLoadingAllowed();
        done.release();
    }};
    EXPECT_TRUE(done.try_acquire_for(std::chrono::milliseconds{1000}));
    t.join();
}

TEST_F(LedgerCacheLoadingStateTest, WaitForLoadingAllowedUnblocksWhenAllowed)
{
    std::binary_semaphore started{0};
    std::binary_semaphore done{0};
    std::thread waiter{[&] {
        started.release();
        state.waitForLoadingAllowed();
        done.release();
    }};

    EXPECT_TRUE(started.try_acquire_for(std::chrono::milliseconds{1000}));
    EXPECT_FALSE(done.try_acquire_for(std::chrono::milliseconds{10}));

    state.allowLoading();
    EXPECT_TRUE(done.try_acquire_for(std::chrono::milliseconds{1000}));
    waiter.join();
}
