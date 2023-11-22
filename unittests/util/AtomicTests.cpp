//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <util/Atomic.h>

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
