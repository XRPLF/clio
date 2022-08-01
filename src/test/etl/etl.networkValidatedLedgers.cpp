#include <gtest/gtest.h>

#include <clio/etl/ETLHelpers.h>
#include <thread>

TEST(networkValidatedLedgers, waitsUntilFirstValidated)
{
    auto nwvl = NetworkValidatedLedgers::make_ValidatedLedgers();

    std::atomic_bool continued = false;
    std::thread t([&]() {
        auto recent = nwvl->getMostRecent();
        continued = true;

        ASSERT_TRUE(recent);
        ASSERT_EQ(*recent, 3);
    });

    nwvl->push(3);
    t.join();

    ASSERT_TRUE(continued);
}

TEST(networkValidatedLedgers, waitsUntilValidated)
{
    auto nwvl = NetworkValidatedLedgers::make_ValidatedLedgers();

    std::mutex mtx;
    std::condition_variable cv;

    std::atomic_bool continued = false;
    std::thread t([&]() {
        auto validated = nwvl->waitUntilValidatedByNetwork(12);
        continued = true;

        ASSERT_TRUE(validated);
    });

    nwvl->push(12);
    t.join();

    ASSERT_TRUE(continued);
}

TEST(networkValidatedLedgers, waitsUntilValidatedOr1ms)
{
    auto nwvl = NetworkValidatedLedgers::make_ValidatedLedgers();

    std::mutex mtx;
    std::condition_variable cv;

    std::atomic_bool continued = false;
    std::thread t([&]() {
        auto validated = nwvl->waitUntilValidatedByNetwork(12, 1);
        continued = true;

        ASSERT_TRUE(validated);
    });

    nwvl->push(12);
    t.join();

    ASSERT_TRUE(continued);
}

TEST(networkValidatedLedgers, timesOutAfter1ms)
{
    auto nwvl = NetworkValidatedLedgers::make_ValidatedLedgers();

    std::mutex mtx;
    std::condition_variable cv;

    std::atomic_bool continued = false;
    std::thread t([&]() {
        auto validated = nwvl->waitUntilValidatedByNetwork(12, 1);
        continued = true;

        ASSERT_FALSE(validated);
    });

    t.join();

    ASSERT_TRUE(continued);
}