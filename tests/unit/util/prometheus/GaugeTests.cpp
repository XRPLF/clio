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
    GaugeInt gauge{
        "test_gauge",
        R"(label1="value1",label2="value2")",
        static_cast<MockGaugeImpl&>(mockGaugeImpl)
    };
};

TEST_F(AnyGaugeTests, operatorAdd)
{
    EXPECT_CALL(mockGaugeImpl, add(1));
    ++gauge;
    EXPECT_CALL(mockGaugeImpl, add(42));
    gauge += 42;
}

TEST_F(AnyGaugeTests, operatorSubtract)
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

TEST_F(GaugeIntTests, operatorSubtract)
{
    --gauge;
    EXPECT_EQ(gauge.value(), -1);
}

TEST_F(GaugeIntTests, set)
{
    gauge.set(21);
    EXPECT_EQ(gauge.value(), 21);
}

TEST_F(GaugeIntTests, multithreadAddAndSubtract)
{
    static constexpr auto kNumAdditions = 1000;
    static constexpr auto kNumNumberAdditions = 100;
    static constexpr auto kNumberToAdd = 11;
    static constexpr auto kNumSubtractions = 2000;
    static constexpr auto kNumNumberSubtractions = 300;
    static constexpr auto kNumberToSubtract = 300;
    std::thread thread1([&] {
        for (int i = 0; i < kNumAdditions; ++i) {
            ++gauge;
        }
    });
    std::thread thread2([&] {
        for (int i = 0; i < kNumNumberAdditions; ++i) {
            gauge += kNumberToAdd;
        }
    });
    std::thread thread3([&] {
        for (int i = 0; i < kNumSubtractions; ++i) {
            --gauge;
        }
    });
    std::thread thread4([&] {
        for (int i = 0; i < kNumNumberSubtractions; ++i) {
            gauge -= kNumberToSubtract;
        }
    });
    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
    EXPECT_EQ(
        gauge.value(),
        kNumAdditions + (kNumNumberAdditions * kNumberToAdd) - kNumSubtractions -
            (kNumNumberSubtractions * kNumberToSubtract)
    );
}

TEST_F(GaugeIntTests, DefaultValue)
{
    GaugeInt const realGauge{"some_gauge", ""};
    EXPECT_EQ(realGauge.value(), 0);
}

struct GaugeDoubleTests : ::testing::Test {
    GaugeDouble gauge{"test_Gauge", R"(label1="value1",label2="value2")"};
};

TEST_F(GaugeDoubleTests, DefaultValue)
{
    GaugeDouble const realGauge{"some_gauge", ""};
    EXPECT_EQ(realGauge.value(), 0.);
}

TEST_F(GaugeDoubleTests, operatorAdd)
{
    ++gauge;
    gauge += 24.1234;
    EXPECT_NEAR(gauge.value(), 25.1234, 1e-9);
}

TEST_F(GaugeDoubleTests, operatorSubtract)
{
    --gauge;
    EXPECT_EQ(gauge.value(), -1.0);
}

TEST_F(GaugeDoubleTests, set)
{
    gauge.set(21.1234);
    EXPECT_EQ(gauge.value(), 21.1234);
}

TEST_F(GaugeDoubleTests, multithreadAddAndSubtract)
{
    static constexpr auto kNumAdditions = 1000;
    static constexpr auto kNumNumberAdditions = 100;
    static constexpr auto kNumberToAdd = 11.1234;
    static constexpr auto kNumSubtractions = 2000;
    static constexpr auto kNumNumberSubtractions = 300;
    static constexpr auto kNumberToSubtract = 300.321;
    std::thread thread1([&] {
        for (int i = 0; i < kNumAdditions; ++i) {
            ++gauge;
        }
    });
    std::thread thread2([&] {
        for (int i = 0; i < kNumNumberAdditions; ++i) {
            gauge += kNumberToAdd;
        }
    });
    std::thread thread3([&] {
        for (int i = 0; i < kNumSubtractions; ++i) {
            --gauge;
        }
    });
    std::thread thread4([&] {
        for (int i = 0; i < kNumNumberSubtractions; ++i) {
            gauge -= kNumberToSubtract;
        }
    });
    thread1.join();
    thread2.join();
    thread3.join();
    thread4.join();
    EXPECT_NEAR(
        gauge.value(),
        kNumAdditions + (kNumNumberAdditions * kNumberToAdd) - kNumSubtractions -
            (kNumNumberSubtractions * kNumberToSubtract),
        1e-9
    );
}
