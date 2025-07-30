//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#pragma once

#include "util/Spawn.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <atomic>
#include <concepts>
#include <csignal>
#include <cstddef>
#include <memory>
#include <utility>

namespace util {

class Coroutine;

/**
 * @brief Concept for functions that can be used as coroutine bodies.
 * Such functions must be invocable with a `Coroutine&` argument.
 * @tparam Fn The function type to check.
 */
template <typename Fn>
concept CoroutineFunction = std::invocable<Fn, Coroutine&> and not std::is_reference_v<Fn>;

/**
 * @brief Manages a coroutine execution context, allowing for cooperative multitasking
 *        and cancellation.
 *
 * The Coroutine class wraps a Boost.Asio yield_context and provides mechanisms
 * for spawning new coroutines, child coroutines, and managing their lifecycle,
 * including cancellation. It integrates with a signal system to propagate
 * cancellation requests across related coroutines.
 */
class Coroutine {
public:
    /**
     * @brief Type alias for a yield_context that is bound to a cancellation slot.
     * This allows asynchronous operations initiated with this context to be cancelled.
     */
    using cancellable_yield_context_type =
        boost::asio::cancellation_slot_binder<boost::asio::yield_context, boost::asio::cancellation_slot>;

private:
    boost::asio::yield_context yield_;
    boost::system::error_code error_;
    boost::asio::cancellation_signal cancellationSignal_;
    cancellable_yield_context_type cyield_;
    std::atomic_bool isCancelled_{false};

    using FamilyCancellationSignal = boost::signals2::signal<void(boost::asio::cancellation_type_t)>;
    std::shared_ptr<FamilyCancellationSignal> familySignal_;
    boost::signals2::connection connection_;

    /**
     * @brief Private constructor to create a Coroutine instance.
     * @param yield The Boost.Asio yield_context for this coroutine.
     * @param signal A shared signal used for propagating cancellation requests among related coroutines.
     */
    explicit Coroutine(
        boost::asio::yield_context&& yield,
        std::shared_ptr<FamilyCancellationSignal> signal = std::make_shared<FamilyCancellationSignal>()
    );

public:
    /**
     * @brief Destructor for the Coroutine.
     * Handles cleanup, such as disconnecting from the cancellation signal.
     */
    ~Coroutine();

    Coroutine(Coroutine const&) = delete;
    Coroutine(Coroutine&&) = delete;

    Coroutine&
    operator==(Coroutine&&) = delete;

    Coroutine&
    operator==(Coroutine const&) = delete;

    /**
     * @brief Spawns a new top-level coroutine.
     * @tparam ExecutionContext The type of the I/O execution context (e.g., boost::asio::io_context).
     * @tparam Fn The type of the invocable function that represents the coroutine body.
     * @param ioContext The I/O execution context on which to spawn the coroutine.
     * @param fn The function to be executed as the coroutine. It will receive a Coroutine& argument.
     */
    template <typename ExecutionContext, CoroutineFunction Fn>
    static void
    spawnNew(ExecutionContext& ioContext, Fn fn)
    {
        util::spawn(ioContext, [fn = std::move(fn)](boost::asio::yield_context yield) {
            Coroutine thisCoroutine{std::move(yield)};
            fn(thisCoroutine);
        });
    }

    /**
     * @brief Spawns a child coroutine from this coroutine.
     * The child coroutine shares the same cancellation signal.
     * @tparam Fn The type of the invocable function that represents the child coroutine body.
     * @param fn The function to be executed as the child coroutine. It will receive a Coroutine& argument.
     */
    template <CoroutineFunction Fn>
    void
    spawnChild(Fn fn)
    {
        if (isCancelled_)
            return;

        util::spawn(yield_, [signal = familySignal_, fn = std::move(fn)](boost::asio::yield_context yield) mutable {
            Coroutine coroutine(std::move(yield), std::move(signal));
            fn(coroutine);
        });
    }

    /**
     * @brief Returns the error code, if any, associated with the last operation in this coroutine.
     * @return A boost::system::error_code indicating the status.
     */
    [[nodiscard]] boost::system::error_code
    error() const;

    /**
     * @brief Cancels all coroutines sharing the same root cancellation signal.
     * @param cancellationType The type of cancellation to perform.
     *                         Defaults to boost::asio::cancellation_type::terminal.
     */
    void
    cancelAll(boost::asio::cancellation_type_t cancellationType = boost::asio::cancellation_type::terminal);

    /**
     * @brief Checks if this coroutine has been cancelled.
     * @return True if the coroutine is cancelled, false otherwise.
     */
    [[nodiscard]] bool
    isCancelled() const;

    /**
     * @brief Returns the cancellable yield context associated with this coroutine.
     * This context should be used for Boost.Asio asynchronous operations within the coroutine
     * to enable cancellation.
     * @return A cancellable_yield_context_type object.
     */
    [[nodiscard]] cancellable_yield_context_type
    yieldContext() const;

    /**
     * @brief Returns the executor associated with this coroutine's yield context.
     * @return The executor.
     */
    [[nodiscard]] boost::asio::any_io_executor
    executor() const;

    /**
     * @brief Explicitly yields execution back to the scheduler.
     * This can be used to allow other tasks to run.
     */
    void
    yield() const;
};

}  // namespace util
