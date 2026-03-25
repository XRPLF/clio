#include "util/Profiler.hpp"

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
