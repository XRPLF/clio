#include "util/Atomic.hpp"

#include <gtest/gtest.h>

#include <thread>

using namespace util;

TEST(AtomicTests, add)
{
    Atomic<int> atomic{42};
    atomic.add(1);
    EXPECT_EQ(atomic.value(), 43);
}

TEST(AtomicTests, set)
{
    Atomic<int> atomic{42};
    atomic.set(1);
    EXPECT_EQ(atomic.value(), 1);
}

TEST(AtomicTest, multithreadAddInt)
{
    Atomic<int> atomic{0};
    std::vector<std::thread> threads;
    threads.reserve(100);
    for (int i = 0; i < 100; ++i) {
        threads.emplace_back([&atomic] {
            for (int j = 0; j < 100; ++j) {
                atomic.add(1);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(atomic.value(), 10000);
}

TEST(AtomicTest, multithreadAddDouble)
{
    Atomic<double> atomic{0.0};
    std::vector<std::thread> threads;
    threads.reserve(100);
    for (int i = 0; i < 100; ++i) {
        threads.emplace_back([&atomic] {
            for (int j = 0; j < 100; ++j) {
                atomic.add(1.0);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_NEAR(atomic.value(), 10000.0, 1e-9);
}
