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
    );

public:
    ~Coroutine();

    Coroutine(Coroutine const&) = delete;
    Coroutine(Coroutine&&) = delete;

    Coroutine&
    operator==(Coroutine&&) = delete;

    Coroutine&
    operator==(Coroutine const&) = delete;

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
    error() const;

    void
    cancel(boost::asio::cancellation_type_t cancellationType = boost::asio::cancellation_type::terminal);

    void
    cancelAll(boost::asio::cancellation_type_t cancellationType = boost::asio::cancellation_type::terminal);

    bool
    isCancelled() const;

    cancellable_yield_context_type
    yieldContext() const;
};

}  // namespace util
