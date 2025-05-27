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
    class Coroutine {
    public:
        using cancellable_yield_context_type =
            boost::asio::cancellation_slot_binder<boost::asio::yield_context, boost::asio::cancellation_slot>;

    private:
        boost::asio::yield_context yield_;
        boost::system::error_code error_;
        boost::asio::cancellation_signal cancellationSignal_;
        cancellable_yield_context_type cyield_;
        size_t generation_;

        using GlobalSignal = boost::signals2::signal<void(size_t, boost::asio::cancellation_type_t)>;
        std::shared_ptr<GlobalSignal> signal_;
        boost::signals2::connection connection_;

        explicit Coroutine(
            boost::asio::yield_context&& yield,
            std::shared_ptr<GlobalSignal> signal = std::make_shared<GlobalSignal>(),
            size_t generation = 0
        )
            : yield_(std::move(yield))
            , cyield_(boost::asio::bind_cancellation_slot(cancellationSignal_.slot(), yield_))
            , generation_{generation}
            , signal_{std::move(signal)}
            , connection_{signal_->connect(
                  [this](size_t generationToCancel, boost::asio::cancellation_type_t cancellationType) {
                      if (generation_ >= generationToCancel) {
                          cancellationSignal_.emit(cancellationType);
                      }
                  }
              )}

        {
        }

    public:
        ~Coroutine()
        {
            // Could have used scoped_connection, but it is better to explicitly show the disconnect
            connection_.disconnect();
        }

        template <typename ExecutionContext, std::invocable<Coroutine&> Fn>
        static void
        spawnNew(ExecutionContext& ioContext, Fn&& fn)
        {
            boost::asio::spawn(ioContext, [fn = std::forward<Fn>(fn)](boost::asio::yield_context yield) {
                Coroutine thisCoroutine{std::move(yield)};
                fn(thisCoroutine);
            });
        }

        template <std::invocable<Coroutine&> Fn>
        void
        spawnChild(Fn&& fn)
        {
            boost::asio::spawn([nextGeneration = generation_ + 1,
                                signal = signal_,
                                fn = std::forward<Fn>(fn)](boost::asio::yield_context yield) mutable {
                Coroutine coroutine(std::move(yield), std::move(signal), nextGeneration);
                fn(coroutine);
            });
        }

        boost::system::error_code
        error() const
        {
            return error_;
        }

        void
        cancel(boost::asio::cancellation_type_t cancellationType = boost::asio::cancellation_type::terminal)
        {
            signal_->operator()(generation_, cancellationType);
        }
        void
        cancelAll(boost::asio::cancellation_type_t cancellationType = boost::asio::cancellation_type::terminal)
        {
            signal_->operator()(0, cancellationType);
        }

        bool
        isCancelled() const
        {
            return error_ == boost::asio::error::operation_aborted;
        }

        cancellable_yield_context_type
        yieldContext() const
        {
            return cyield_;
        }
    };

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
