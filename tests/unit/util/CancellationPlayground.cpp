#include "util/AsioContextTestFixture.hpp"

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>
#include <print>
#include <thread>
#include <utility>

// spawn on yield
// copy yield

template <typename... Args>
static void
print(char const* s, Args&&... args)
{
    std::print("{:%S}: ", std::chrono::system_clock::now());
    fmt::println(fmt::runtime(s), std::forward<Args>(args)...);
}

struct CancellationPlaygroundTest : SyncAsioContextTest {
    struct Context {
        boost::system::error_code error;
        boost::asio::cancellation_signal cancellationSignal;
        std::optional<boost::asio::yield_context> yield;

        void
        cancel()
        {
            cancellationSignal.emit(boost::asio::cancellation_type::terminal);
        }

        auto
        cancellableYield()
        {
            return boost::asio::bind_cancellation_slot(cancellationSignal.slot(), (*yield)[error]);
        }

        bool
        isCancelled() const
        {
            return error == boost::asio::error::operation_aborted;
        }
    };

    void
    doWork(Context& context)
    {
        auto yield = context.cancellableYield();

        asyncOperation(yield, 100);
        if (context.isCancelled())
            return;

        boost::asio::spawn(yield, [this](boost::asio::yield_context innerYield) {
            print("Inner operation started");
            asyncOperation(innerYield, 200);
            print("Inner operation finished");
        });

        asyncOperation(yield, 100);
        if (context.isCancelled())
            return;

        asyncOperation(yield, 100);
        if (context.isCancelled())
            return;

        asyncOperation(yield, 100);
        if (context.isCancelled())
            return;
    }

    static std::atomic_size_t kCOUNTER;
    void
    asyncOperation(auto yield, int durationMs)
    {
        auto const id = kCOUNTER.fetch_add(1);

        print("starting asyncOperation {}", id);

        if constexpr (requires { yield.get(); }) {
            auto ioContext = yield.get().get_executor();
            boost::asio::steady_timer timer{ioContext, std::chrono::milliseconds{durationMs}};
            timer.async_wait(yield);
        } else {
            auto ioContext = yield.get_executor();
            boost::asio::steady_timer timer{ioContext, std::chrono::milliseconds{durationMs}};
            timer.async_wait(yield);
        }

        std::println("{:%S}: finished asyncOperation {}", std::chrono::system_clock::now(), id);
    }
};

std::atomic_size_t CancellationPlaygroundTest::kCOUNTER = 1;

TEST_F(CancellationPlaygroundTest, cancellation_signal)
{
    Context context;
    std::thread t{[&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds{150});
        print("Cancelling");
        context.cancel();
        print("Cancelled");
    }};
    runSpawn([&](boost::asio::yield_context yield) {
        print("Run spawn started");
        context.yield = std::move(yield);
        doWork(context);
        print("Run spawn finished");
    });
    t.join();
}
