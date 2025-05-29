#include "util/AsioContextTestFixture.hpp"

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/signals2/variadic_signal.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <memory>
#include <print>
#include <thread>
#include <utility>

template <typename... Args>
static void
print(char const* s, Args&&... args)
{
    std::print("{:%S}: ", std::chrono::system_clock::now());
    fmt::println(fmt::runtime(s), std::forward<Args>(args)...);
}

struct CancellationPlaygroundTest : SyncAsioContextTest {
    size_t
    doWork(Coroutine& coroutine)
    {
        auto yield = coroutine.yieldContext();

        asyncOperation(yield, 100);
        if (coroutine.isCancelled())
            return kCOUNTER;

        coroutine.spawnChild([this](Coroutine& coroutine) {
            print("Inner operation started");
            asyncOperation(coroutine.yieldContext(), 200);
            print("Inner operation finished");
        });

        asyncOperation(yield, 100);
        if (coroutine.isCancelled())
            return kCOUNTER;

        asyncOperation(yield, 100);
        if (coroutine.isCancelled())
            return kCOUNTER;

        asyncOperation(yield, 100);
        return kCOUNTER;
    }

    static std::atomic_size_t kCOUNTER;
    void
    asyncOperation(auto yield, int durationMs)
    {
        auto const id = kCOUNTER.fetch_add(1);

        print("starting asyncOperation {}", id);

        auto ioContext = yield.get().get_executor();
        boost::asio::steady_timer timer{ioContext, std::chrono::milliseconds{durationMs}};
        timer.async_wait(yield);

        std::println("{:%S}: finished asyncOperation {}", std::chrono::system_clock::now(), id);
    }
};

std::atomic_size_t CancellationPlaygroundTest::kCOUNTER = 1;

TEST_F(CancellationPlaygroundTest, cancellation_signal)
{
    Coroutine::spawnNew(ctx_, [this](Coroutine& coroutine) {
        print("Run spawn started");
        coroutine.spawnChild([](Coroutine& coroutine) {
            print("Spawned cancelling");
            boost::asio::steady_timer timer(
                coroutine.yieldContext().get().get_executor(), std::chrono::milliseconds{150}
            );
            timer.async_wait(coroutine.yieldContext());
            print("Cancelling");
            coroutine.cancelAll();
            print("Cancelling done");
        });
        auto const result = doWork(coroutine) - 1;
        print("doWork() finished after {}th operation", result);
        print("Run spawn finished");
    });
    ctx_.run();
}
