//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2026, the clio developers.

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
