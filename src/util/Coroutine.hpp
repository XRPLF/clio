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

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <concepts>
#include <csignal>
#include <cstddef>
#include <memory>
#include <utility>

namespace util {

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
    size_t generation_;

    using GlobalSignal = boost::signals2::signal<void(size_t, boost::asio::cancellation_type_t)>;
    std::shared_ptr<GlobalSignal> signal_;
    boost::signals2::connection connection_;

    /**
     * @brief Private constructor to create a Coroutine instance.
     * @param yield The Boost.Asio yield_context for this coroutine.
     * @param signal A shared signal used for propagating cancellation requests among related coroutines.
     * @param generation The generation number of this coroutine, used to manage parent/child relationships.
     */
    explicit Coroutine(
        boost::asio::yield_context&& yield,
        std::shared_ptr<GlobalSignal> signal = std::make_shared<GlobalSignal>(),
        size_t generation = 0
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
    template <typename ExecutionContext, std::invocable<Coroutine&> Fn>
    static void
    spawnNew(ExecutionContext& ioContext, Fn&& fn)
    {
        boost::asio::spawn(ioContext, [fn = std::forward<Fn>(fn)](boost::asio::yield_context yield) {
            Coroutine thisCoroutine{std::move(yield)};
            fn(thisCoroutine);
        });
    }

    /**
     * @brief Spawns a child coroutine from this coroutine.
     * The child coroutine shares the same cancellation signal and has an incremented generation number.
     * @tparam Fn The type of the invocable function that represents the child coroutine body.
     * @param fn The function to be executed as the child coroutine. It will receive a Coroutine& argument.
     */
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

    /**
     * @brief Returns the error code, if any, associated with the last operation in this coroutine.
     * @return A boost::system::error_code indicating the status.
     */
    boost::system::error_code
    error() const;

    /**
     * @brief Cancels this specific coroutine and its direct children.
     * @param cancellationType The type of cancellation to perform (e.g., terminal, partial).
     *                         Defaults to boost::asio::cancellation_type::terminal.
     */
    void
    cancel(boost::asio::cancellation_type_t cancellationType = boost::asio::cancellation_type::terminal);

    /**
     * @brief Cancels this coroutine, all its children, and all related coroutines (siblings, parent).
     * This effectively cancels all coroutines sharing the same root cancellation signal.
     * @param cancellationType The type of cancellation to perform.
     *                         Defaults to boost::asio::cancellation_type::terminal.
     */
    void
    cancelAll(boost::asio::cancellation_type_t cancellationType = boost::asio::cancellation_type::terminal);

    /**
     * @brief Checks if this coroutine has been cancelled.
     * @return True if the coroutine is cancelled, false otherwise.
     */
    bool
    isCancelled() const;

    /**
     * @brief Returns the cancellable yield context associated with this coroutine.
     * This context should be used for Boost.Asio asynchronous operations within the coroutine
     * to enable cancellation.
     * @return A cancellable_yield_context_type object.
     */
    cancellable_yield_context_type
    yieldContext() const;
};

}  // namespace util
