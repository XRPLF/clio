#include "util/ObservableValue.hpp"

#include <boost/signals2/connection.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace testing;
using namespace util;

namespace {

}  // namespace

class ObservableValueAtomicTest : public ::testing::Test {};

TEST_F(ObservableValueAtomicTest, BasicConstruction)
{
    ObservableValue<std::atomic<int>> const obs{42};

    EXPECT_EQ(obs.get(), 42);
    EXPECT_EQ(static_cast<int>(obs), 42);
    EXPECT_FALSE(obs.hasObservers());
}

TEST_F(ObservableValueAtomicTest, DefaultConstruction)
{
    ObservableValue<std::atomic<int>> const obsInt;
    EXPECT_EQ(obsInt.get(), 0);

    ObservableValue<std::atomic<bool>> const obsBool;
    EXPECT_FALSE(obsBool.get());

    EXPECT_FALSE(obsInt.hasObservers());
    EXPECT_FALSE(obsBool.hasObservers());
}

TEST_F(ObservableValueAtomicTest, BasicObservation)
{
    ObservableValue<std::atomic<int>> obs{10};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(20));
    obs = 20;
    EXPECT_EQ(obs.get(), 20);
}

TEST_F(ObservableValueAtomicTest, SetMethod)
{
    ObservableValue<std::atomic<int>> obs{5};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(15));
    obs.set(15);
    EXPECT_EQ(obs.get(), 15);

    obs.set(15);  // Same value should not notify
    EXPECT_EQ(obs.get(), 15);
}

TEST_F(ObservableValueAtomicTest, AtomicBasicUsage)
{
    ObservableValue<std::atomic<int>> obs{10};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(25));
    obs.set(25);

    EXPECT_EQ(obs.get(), 25);
}

TEST_F(ObservableValueAtomicTest, AtomicMultipleChanges)
{
    ObservableValue<std::atomic<int>> obs{50};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(100));  // First change: 50→100
    EXPECT_CALL(mockObserver, Call(50));   // Second change: 100→50
    obs.set(100);                          // Should notify: 50→100
    obs.set(50);                           // Should notify: 100→50

    EXPECT_EQ(obs.get(), 50);
}

TEST_F(ObservableValueAtomicTest, AtomicNoChangeNoNotification)
{
    ObservableValue<std::atomic<int>> obs{42};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    // No EXPECT_CALL since no notification should occur
    obs.set(42);  // Same value, should not notify
    obs.set(42);  // Same value again, should not notify

    EXPECT_EQ(obs.get(), 42);
}

TEST_F(ObservableValueAtomicTest, AtomicSequentialChanges)
{
    ObservableValue<std::atomic<int>> obs{1};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(2));
    obs.set(2);

    EXPECT_CALL(mockObserver, Call(3));
    obs.set(3);

    EXPECT_EQ(obs.get(), 3);
}

TEST_F(ObservableValueAtomicTest, MultipleObservers)
{
    ObservableValue<std::atomic<int>> obs{0};

    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver1;
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver2;

    auto conn1 = obs.observe(mockObserver1.AsStdFunction());
    auto conn2 = obs.observe(mockObserver2.AsStdFunction());

    EXPECT_CALL(mockObserver1, Call(42));
    EXPECT_CALL(mockObserver2, Call(42));
    obs = 42;

    conn1.disconnect();
    EXPECT_CALL(mockObserver2, Call(100));
    obs = 100;
}

TEST_F(ObservableValueAtomicTest, ThreadSafetyBasic)
{
    ObservableValue<std::atomic<int>> obs{0};
    std::atomic<int> notificationCount{0};
    std::vector<int> values;
    std::mutex valuesMutex;

    auto connection = obs.observe([&](int const& value) {
        notificationCount.fetch_add(1);
        std::scoped_lock const lock(valuesMutex);
        values.push_back(value);
    });

    static constexpr auto kNUM_THREADS = 4;
    static constexpr auto kINCREMENTS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    threads.reserve(kNUM_THREADS);

    for (int i = 0; i < kNUM_THREADS; ++i) {
        threads.emplace_back([&obs]() {
            for (int j = 0; j < kINCREMENTS_PER_THREAD; ++j) {
                int const expected = obs.get();
                int const newValue = expected + 1;
                obs.set(newValue);
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }

    for (auto& thread : threads)
        thread.join();

    // Final value may be less than kNumThreads * kIncrementsPerThread due to race conditions
    EXPECT_GT(obs.get(), 0);
    EXPECT_GT(notificationCount.load(), 0);

    std::scoped_lock const lock(valuesMutex);
    for (auto const& value : values) {
        EXPECT_GT(value, 0);
    }
}

TEST_F(ObservableValueAtomicTest, ThreadSafetyWithDirectAccess)
{
    ObservableValue<std::atomic<int>> obs{0};
    std::atomic<int> notificationCount{0};

    auto connection = obs.observe([&](int const&) { notificationCount.fetch_add(1); });

    static constexpr auto kNUM_THREADS = 4;
    static constexpr auto kOPERATIONS_PER_THREAD = 50;

    std::vector<std::thread> threads;
    threads.reserve(kNUM_THREADS);

    for (int i = 0; i < kNUM_THREADS; ++i) {
        threads.emplace_back([&obs]() {
            for (int j = 0; j < kOPERATIONS_PER_THREAD; ++j) {
                int const current = obs.get();
                obs.set(current + 1);
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }

    for (auto& thread : threads)
        thread.join();

    EXPECT_GT(obs.get(), 0);
    EXPECT_GT(notificationCount.load(), 0);
}

TEST_F(ObservableValueAtomicTest, AtomicBoolSpecialization)
{
    ObservableValue<std::atomic<bool>> obs{false};
    testing::StrictMock<testing::MockFunction<void(bool const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(true));
    obs = true;
    EXPECT_TRUE(obs.get());

    obs = true;  // Same value should not notify

    EXPECT_CALL(mockObserver, Call(false));
    obs.set(false);
    EXPECT_FALSE(obs.get());
}

TEST_F(ObservableValueAtomicTest, CompareAndSwapBehavior)
{
    ObservableValue<std::atomic<int>> obs{10};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    // Test that compare-and-swap works correctly in set()
    obs.set(10);  // Same value, should not notify

    EXPECT_CALL(mockObserver, Call(20));
    obs.set(20);  // Different value, should notify
}

TEST_F(ObservableValueAtomicTest, RaceConditionNotificationIntegrity)
{
    ObservableValue<std::atomic<int>> obs{0};
    std::atomic<int> notificationCount{0};
    std::vector<int> values;
    std::mutex valuesMutex;

    auto connection = obs.observe([&](int const& value) {
        notificationCount.fetch_add(1);
        std::scoped_lock const lock(valuesMutex);
        values.push_back(value);
    });

    static constexpr auto kNUM_THREADS = 10;
    static constexpr auto kOPERATIONS_PER_THREAD = 20;

    std::vector<std::thread> threads;
    threads.reserve(kNUM_THREADS);

    for (int i = 0; i < kNUM_THREADS; ++i) {
        threads.emplace_back([&obs]() {
            for (int j = 0; j < kOPERATIONS_PER_THREAD; ++j) {
                obs.set(j % 3);
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });
    }

    for (auto& thread : threads)
        thread.join();

    EXPECT_GT(notificationCount.load(), 0);

    std::scoped_lock const lock(valuesMutex);
    for (auto const& value : values) {
        EXPECT_GE(value, 0);
        EXPECT_LE(value, 2);
    }

    int const finalValue = obs.get();
    EXPECT_GE(finalValue, 0);
    EXPECT_LE(finalValue, 2);
}

TEST_F(ObservableValueAtomicTest, DeterministicNotificationTest)
{
    ObservableValue<std::atomic<int>> obs{0};
    std::atomic<int> notificationCount{0};
    std::vector<int> values;
    std::mutex valuesMutex;

    auto connection = obs.observe([&](int const& value) {
        notificationCount.fetch_add(1);
        std::scoped_lock const lock(valuesMutex);
        values.push_back(value);
    });

    static constexpr auto kNUM_THREADS = 5;

    std::vector<std::thread> threads;
    threads.reserve(kNUM_THREADS);

    for (int i = 0; i < kNUM_THREADS; ++i) {
        threads.emplace_back([&obs, i]() { obs.set(i + 1); });
    }

    for (auto& thread : threads)
        thread.join();

    // Each thread sets a unique value, so expect exactly kNumThreads notifications
    EXPECT_EQ(notificationCount.load(), kNUM_THREADS);

    std::scoped_lock const lock(valuesMutex);
    EXPECT_EQ(values.size(), kNUM_THREADS);

    for (auto const& value : values) {
        EXPECT_GE(value, 1);
        EXPECT_LE(value, kNUM_THREADS);
    }

    int const finalValue = obs.get();
    EXPECT_GE(finalValue, 1);
    EXPECT_LE(finalValue, kNUM_THREADS);
}

TEST_F(ObservableValueAtomicTest, NoNotificationForSameValue)
{
    ObservableValue<std::atomic<int>> obs{42};
    std::atomic<int> notificationCount{0};

    auto connection = obs.observe([&](int const&) { notificationCount.fetch_add(1); });

    static constexpr auto kNUM_THREADS = 10;

    std::vector<std::thread> threads;
    threads.reserve(kNUM_THREADS);

    for (int i = 0; i < kNUM_THREADS; ++i) {
        threads.emplace_back([&obs]() { obs.set(42); });
    }

    for (auto& thread : threads)
        thread.join();

    EXPECT_EQ(notificationCount.load(), 0);  // No notifications since value never changed
    EXPECT_EQ(obs.get(), 42);
}

TEST_F(ObservableValueAtomicTest, AtomicRaceConditionCorrectness)
{
    ObservableValue<std::atomic<int>> obs{0};
    std::atomic<int> notificationCount{0};
    std::vector<int> values;
    std::mutex valuesMutex;

    auto connection = obs.observe([&](int const& value) {
        notificationCount.fetch_add(1);
        std::scoped_lock const lock(valuesMutex);
        values.push_back(value);
    });

    static constexpr auto kNUM_THREADS = 3;

    std::vector<std::thread> threads;
    threads.reserve(kNUM_THREADS);

    // Test that direct access properly notifies for all value changes
    // Each thread will make unique changes to avoid race condition conflicts
    for (int i = 0; i < kNUM_THREADS; ++i) {
        threads.emplace_back([&obs, i]() {
            int const baseValue = (i + 1) * 10;  // 10, 20, 30
            obs.set(baseValue);                  // Store unique values
            obs.set(baseValue + 1);              // Then increment
        });
    }

    for (auto& thread : threads)
        thread.join();

    // We should get some notifications (exact count depends on race conditions)
    // but at least one per thread since they use unique base values
    EXPECT_GE(notificationCount.load(), kNUM_THREADS);

    std::scoped_lock const lock(valuesMutex);
    EXPECT_GE(values.size(), kNUM_THREADS);

    for (auto const& value : values)
        EXPECT_GT(value, 0);
}

TEST_F(ObservableValueAtomicTest, ForceNotify)
{
    ObservableValue<std::atomic<int>> obs{42};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    obs.forceNotify();

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(42));
    obs.forceNotify();

    EXPECT_CALL(mockObserver, Call(42));
    obs.forceNotify();

    EXPECT_CALL(mockObserver, Call(100));
    obs.set(100);
    EXPECT_CALL(mockObserver, Call(100));
    obs.forceNotify();

    EXPECT_CALL(mockObserver, Call(100)).Times(3);
    obs.forceNotify();
    obs.forceNotify();
    obs.forceNotify();
}
