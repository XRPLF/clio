#pragma once

#include "util/Assert.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <memory>
#include <semaphore>
#include <utility>

namespace util {

/**
 * @brief A class to repeat some action at a regular interval
 * @note io_context must be stopped before the Repeat object is destroyed. Otherwise it is undefined
 * behavior
 */
class Repeat {
    struct Control {
        boost::asio::steady_timer timer;
        boost::asio::strand<boost::asio::any_io_executor> strand;
        std::atomic_bool stopping{true};
        std::binary_semaphore semaphore{0};

        Control(auto& ctx) : timer(ctx), strand(boost::asio::make_strand(ctx))
        {
        }
    };

    std::shared_ptr<Control> control_;

public:
    /**
     * @brief Construct a new Repeat object
     * @note The `ctx` parameter is `auto` so that this util supports `strand` and `thread_pool` as
     * well as `io_context`
     *
     * @param ctx The io_context-like object to use
     */
    Repeat(auto& ctx) : control_(std::make_unique<Control>(ctx))
    {
    }

    Repeat(Repeat const&) = delete;
    Repeat&
    operator=(Repeat const&) = delete;
    Repeat(Repeat&&) = default;
    Repeat&
    operator=(Repeat&&) = default;

    /**
     * @brief Stop repeating
     * @note This method will block to ensure the repeating is actually stopped. But blocking time
     * should be very short.
     */
    void
    stop();

    /**
     * @brief Start asynchronously repeating
     * @note stop() must be called before start() is called for the second time
     *
     * @tparam Action The action type
     * @param interval The interval to repeat
     * @param action The action to call regularly
     */
    template <std::invocable Action>
    void
    start(std::chrono::steady_clock::duration interval, Action&& action)
    {
        ASSERT(control_->stopping, "Should be stopped before starting");
        control_->stopping = false;
        startImpl(control_, interval, std::forward<Action>(action));
    }

private:
    template <std::invocable Action>
    static void
    startImpl(
        std::shared_ptr<Control> control,
        std::chrono::steady_clock::duration interval,
        Action&& action
    )
    {
        boost::asio::post(
            control->strand, [control, interval, action = std::forward<Action>(action)]() mutable {
                if (control->stopping) {
                    control->semaphore.release();
                    return;
                }

                control->timer.expires_after(interval);
                control->timer.async_wait(
                    [control,
                     interval,
                     action = std::forward<Action>(action)](auto const& ec) mutable {
                        if (ec or control->stopping) {
                            control->semaphore.release();
                            return;
                        }
                        action();

                        startImpl(std::move(control), interval, std::forward<Action>(action));
                    }
                );
            }
        );
    }
};

}  // namespace util
