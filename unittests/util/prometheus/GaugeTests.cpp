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

#include "util/prometheus/Gauge.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <thread>

using namespace util::prometheus;

struct AnyGaugeTests : ::testing::Test {
    struct MockGaugeImpl {
        using ValueType = std::int64_t;
        MOCK_METHOD(void, add, (ValueType));
        MOCK_METHOD(void, set, (ValueType));
        MOCK_METHOD(ValueType, value, (), (const));
    };

    ::testing::StrictMock<MockGaugeImpl> mockGaugeImpl;
    GaugeInt gauge{"test_gauge", R"(label1="value1",label2="value2")", static_cast<MockGaugeImpl&>(mockGaugeImpl)};
};

TEST_F(AnyGaugeTests, operatorAdd)
{
    EXPECT_CALL(mockGaugeImpl, add(1));
    ++gauge;
    EXPECT_CALL(mockGaugeImpl, add(42));
    gauge += 42;
}

TEST_F(AnyGaugeTests, operatorSubstract)
{
    EXPECT_CALL(mockGaugeImpl, add(-1));
    --gauge;
    EXPECT_CALL(mockGaugeImpl, add(-42));
    gauge -= 42;
}

TEST_F(AnyGaugeTests, set)
{
    EXPECT_CALL(mockGaugeImpl, set(42));
    gauge.set(42);
}

TEST_F(AnyGaugeTests, value)
{
    EXPECT_CALL(mockGaugeImpl, value()).WillOnce(::testing::Return(42));
    EXPECT_EQ(gauge.value(), 42);
}

struct GaugeIntTests : ::testing::Test {
    GaugeInt gauge{"test_Gauge", R"(label1="value1",label2="value2")"};
};

TEST_F(GaugeIntTests, operatorAdd)
{
    ++gauge;
    gauge += 24;
    EXPECT_EQ(gauge.value(), 25);
}

TEST_F(GaugeIntTests, operatorSubstract)
{
    --gauge;
    EXPECT_EQ(gauge.value(), -1);
}

TEST_F(GaugeIntTests, set)
{
    gauge.set(21);
    EXPECT_EQ(gauge.value(), 21);
}

TEST_F(GaugeIntTests, multithreadAddAndSubstract)
{
    static constexpr auto numAdditions = 1000;
    static constexpr auto numNumberAdditions = 100;
    static constexpr auto numberToAdd = 11;
    static constexpr auto numSubstractions = 2000;
    static constexpr auto numNumberSubstractions = 300;
    static constexpr auto numberToSubstract = 300;
    std::thread thread1([&] {
        for (int i = 0; i < numAdditions; ++i) {
            ++gauge;
        }
    });
    std::thread thread2([&] {
        for (int i = 0; i < numNumberAdditions; ++i) {
            gauge += numberToAdd;
        }
    });
    std::thread thread3([&] {
        for (int i = 0; i < numSubstractions; ++i) {
            --gauge;
        }
    });
    std::thread thread4([&] {
        for (int i = 0; i < numNumberSubstractions; ++i) {
            gauge -= numberToSubstract;
        }
    });
    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
    EXPECT_EQ(
        gauge.value(),
        numAdditions + numNumberAdditions * numberToAdd - numSubstractions - numNumberSubstractions * numberToSubstract
    );
}

TEST_F(GaugeIntTests, DefaultValue)
{
    GaugeInt realGauge{"some_gauge", ""};
    EXPECT_EQ(realGauge.value(), 0);
}

struct GaugeDoubleTests : ::testing::Test {
    GaugeDouble gauge{"test_Gauge", R"(label1="value1",label2="value2")"};
};

TEST_F(GaugeDoubleTests, DefaultValue)
{
    GaugeDouble realGauge{"some_gauge", ""};
    EXPECT_EQ(realGauge.value(), 0.);
}

TEST_F(GaugeDoubleTests, operatorAdd)
{
    ++gauge;
    gauge += 24.1234;
    EXPECT_NEAR(gauge.value(), 25.1234, 1e-9);
}

TEST_F(GaugeDoubleTests, operatorSubstract)
{
    --gauge;
    EXPECT_EQ(gauge.value(), -1.0);
}

TEST_F(GaugeDoubleTests, set)
{
    gauge.set(21.1234);
    EXPECT_EQ(gauge.value(), 21.1234);
}

TEST_F(GaugeDoubleTests, multithreadAddAndSubstract)
{
    static constexpr auto numAdditions = 1000;
    static constexpr auto numNumberAdditions = 100;
    static constexpr auto numberToAdd = 11.1234;
    static constexpr auto numSubstractions = 2000;
    static constexpr auto numNumberSubstractions = 300;
    static constexpr auto numberToSubstract = 300.321;
    std::thread thread1([&] {
        for (int i = 0; i < numAdditions; ++i) {
            ++gauge;
        }
    });
    std::thread thread2([&] {
        for (int i = 0; i < numNumberAdditions; ++i) {
            gauge += numberToAdd;
        }
    });
    std::thread thread3([&] {
        for (int i = 0; i < numSubstractions; ++i) {
            --gauge;
        }
    });
    std::thread thread4([&] {
        for (int i = 0; i < numNumberSubstractions; ++i) {
            gauge -= numberToSubstract;
        }
    });
    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
    EXPECT_NEAR(
        gauge.value(),
        numAdditions + numNumberAdditions * numberToAdd - numSubstractions - numNumberSubstractions * numberToSubstract,
        1e-9
    );
}
