#include <gtest/gtest.h>

#include <clio/etl/ETLHelpers.h>
#include <thread>

TEST(threadSafeQueue, tryPopFailsWhenEmpty)
{
    ThreadSafeQueue<int> tsq;

    ASSERT_FALSE(tsq.tryPop());
}

TEST(threadSafeQueue, tryPopSucceedsWhenFull)
{
    ThreadSafeQueue<int> tsq;

    tsq.push(3);
    auto opt = tsq.tryPop();
    ASSERT_TRUE(opt);
    ASSERT_EQ(*opt, 3);
}

TEST(threadSafeQueue, popWaitUntilFull)
{
    ThreadSafeQueue<int> tsq;

    bool continued = false;
    auto t = std::thread([&]() {
        auto value = tsq.pop();
        continued = true;

        ASSERT_EQ(value, 3);
    });

    tsq.push(3);
    t.join();

    ASSERT_TRUE(continued);
}

TEST(threadSafeQueue, waitsAtMaxSize)
{
    ThreadSafeQueue<int> tsq(1);

    tsq.push(1);

    bool popped = false;
    bool pushed = false;
    std::mutex mtx;
    auto t = std::thread([&]() {
        {
            std::unique_lock lk(mtx);
            ASSERT_EQ(tsq.pop(), 1);
            ASSERT_FALSE(pushed);
            popped = true;
        }
    });

    // We should only be able to push(2) once we've popped 1
    tsq.push(2);

    {
        std::unique_lock lk(mtx);
        pushed = true;
        ASSERT_TRUE(popped);
    }

    t.join();
}