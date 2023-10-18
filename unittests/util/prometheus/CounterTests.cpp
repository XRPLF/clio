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
#include <util/prometheus/Counter.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <thread>

using namespace util::prometheus;

struct AnyCounterTests : ::testing::Test
{
    struct MockCounterImpl
    {
        using ValueType = std::uint64_t;
        MOCK_METHOD(void, add, (ValueType));
        MOCK_METHOD(void, set, (ValueType));
        MOCK_METHOD(ValueType, value, (), (const));
    };

    ::testing::StrictMock<MockCounterImpl> mockCounterImpl;
    const std::string name = "test_counter";
    const std::string labelsString = R"({label1="value1",label2="value2"})";
    CounterInt counter{name, labelsString, static_cast<MockCounterImpl&>(mockCounterImpl)};
};

TEST_F(AnyCounterTests, name)
{
    EXPECT_EQ(counter.name(), name);
}

TEST_F(AnyCounterTests, labelsString)
{
    EXPECT_EQ(counter.labelsString(), labelsString);
}

TEST_F(AnyCounterTests, serialize)
{
    EXPECT_CALL(mockCounterImpl, value()).WillOnce(::testing::Return(42));
    std::string serialized;
    counter.serialize(serialized);
    EXPECT_EQ(serialized, R"(test_counter{label1="value1",label2="value2"} 42)");
}

TEST_F(AnyCounterTests, operatorAdd)
{
    EXPECT_CALL(mockCounterImpl, add(1));
    ++counter;
    EXPECT_CALL(mockCounterImpl, add(42));
    counter += 42;
}

TEST_F(AnyCounterTests, reset)
{
    EXPECT_CALL(mockCounterImpl, set(0));
    counter.reset();
}

TEST_F(AnyCounterTests, value)
{
    EXPECT_CALL(mockCounterImpl, value()).WillOnce(::testing::Return(42));
    EXPECT_EQ(counter.value(), 42);
}

struct CounterIntTests : ::testing::Test
{
    CounterInt counter{"test_counter", R"(label1="value1",label2="value2")"};
};

TEST_F(CounterIntTests, operatorAdd)
{
    ++counter;
    counter += 24;
    EXPECT_EQ(counter.value(), 25);
}

TEST_F(CounterIntTests, reset)
{
    ++counter;
    EXPECT_EQ(counter.value(), 1);
    counter.reset();
    EXPECT_EQ(counter.value(), 0);
}

TEST_F(CounterIntTests, multithreadAdd)
{
    static constexpr auto numAdditions = 1000;
    static constexpr auto numNumberAdditions = 100;
    static constexpr auto numberToAdd = 11;
    std::thread thread1([&] {
        for (int i = 0; i < numAdditions; ++i)
        {
            ++counter;
        }
    });
    std::thread thread2([&] {
        for (int i = 0; i < numNumberAdditions; ++i)
        {
            counter += numberToAdd;
        }
    });
    thread1.join();
    thread2.join();
    EXPECT_EQ(counter.value(), numAdditions + numNumberAdditions * numberToAdd);
}

struct CounterDoubleTests : ::testing::Test
{
    CounterDouble counter{"test_counter", R"(label1="value1",label2="value2")"};
};

TEST_F(CounterDoubleTests, operatorAdd)
{
    ++counter;
    counter += 24.1234;
    EXPECT_NEAR(counter.value(), 25.1234, 1e-9);
}

TEST_F(CounterDoubleTests, reset)
{
    ++counter;
    EXPECT_EQ(counter.value(), 1.);
    counter.reset();
    EXPECT_EQ(counter.value(), 0.);
}

TEST_F(CounterDoubleTests, multithreadAdd)
{
    static constexpr auto numAdditions = 1000;
    static constexpr auto numNumberAdditions = 100;
    static constexpr auto numberToAdd = 11.1234;
    std::thread thread1([&] {
        for (int i = 0; i < numAdditions; ++i)
        {
            ++counter;
        }
    });
    std::thread thread2([&] {
        for (int i = 0; i < numNumberAdditions; ++i)
        {
            counter += numberToAdd;
        }
    });
    thread1.join();
    thread2.join();
    EXPECT_NEAR(counter.value(), numAdditions + numNumberAdditions * numberToAdd, 1e-9);
}
