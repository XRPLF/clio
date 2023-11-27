//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "util/Profiler.h"

#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <thread>
#include <utility>

using namespace util;
TEST(TimedTest, HasReturnValue)
{
    auto [ret, time] = timed([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return 8;
    });

    ASSERT_EQ(ret, 8);
    ASSERT_NE(time, 0);
}

TEST(TimedTest, ReturnVoid)
{
    auto time = timed([]() { std::this_thread::sleep_for(std::chrono::milliseconds(5)); });

    ASSERT_NE(time, 0);
}

struct FunctorTest {
    void
    operator()() const
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
};

TEST(TimedTest, Functor)
{
    auto time = timed(FunctorTest());

    ASSERT_NE(time, 0);
}

TEST(TimedTest, MovedLambda)
{
    auto f = []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return 8;
    };
    auto [ret, time] = timed(std::move(f));

    ASSERT_EQ(ret, 8);
    ASSERT_NE(time, 0);
}

TEST(TimedTest, ChangeToNs)
{
    auto f = []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return 8;
    };
    auto [ret, time] = timed<std::chrono::nanoseconds>(std::move(f));
    ASSERT_EQ(ret, 8);
    ASSERT_GE(time, 5 * 1000000);
}

TEST(TimedTest, NestedLambda)
{
    double timeNested = std::numeric_limits<double>::quiet_NaN();
    auto f = [&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        timeNested = timed([]() { std::this_thread::sleep_for(std::chrono::milliseconds(5)); });
        return 8;
    };
    auto [ret, time] = timed<std::chrono::nanoseconds>(std::move(f));
    ASSERT_EQ(ret, 8);
    ASSERT_GE(timeNested, 5);
    ASSERT_GE(time, 10 * 1000000);
}

TEST(TimedTest, FloatSec)
{
    auto f = []() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        return 8;
    };
    auto [ret, time] = timed<std::chrono::duration<double>>(std::move(f));
    ASSERT_EQ(ret, 8);
    ASSERT_GE(time, 0);
}
