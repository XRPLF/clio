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

#include "util/Assert.hpp"
#include "util/Spawn.hpp"

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <semaphore>

namespace cluster::impl {

// TODO: Try to replace util::Repeat by this. https://github.com/XRPLF/clio/issues/2926
template <typename Context>
class RepeatedTask {
    std::chrono::steady_clock::duration interval_;
    boost::asio::strand<typename Context::executor_type> strand_;

    enum class State { Running, Stopped };
    std::atomic<State> state_ = State::Stopped;

    std::binary_semaphore semaphore_{0};
    boost::asio::steady_timer timer_;

public:
    RepeatedTask(std::chrono::steady_clock::duration interval, Context& ctx)
        : interval_(interval), strand_(boost::asio::make_strand(ctx)), timer_(strand_)
    {
    }

    ~RepeatedTask()
    {
        stop();
    }

    template <typename Fn>
        requires std::invocable<Fn, boost::asio::yield_context> or std::invocable<Fn>
    void
    run(Fn&& f)
    {
        ASSERT(state_ == State::Stopped, "Can only be ran once");
        state_ = State::Running;
        util::spawn(strand_, [this, f = std::forward<Fn>(f)](boost::asio::yield_context yield) {
            boost::system::error_code ec;

            while (state_ == State::Running) {
                timer_.expires_after(interval_);
                timer_.async_wait(yield[ec]);

                if (ec or state_ != State::Running)
                    break;

                if constexpr (std::invocable<decltype(f), boost::asio::yield_context>) {
                    f(yield);
                } else {
                    f();
                }
            }

            semaphore_.release();
        });
    }

    void
    stop()
    {
        if (auto expected = State::Running; not state_.compare_exchange_strong(expected, State::Stopped))
            return;  // Already stopped or not started

        std::binary_semaphore cancelSemaphore{0};
        boost::asio::post(strand_, [this, &cancelSemaphore]() {
            timer_.cancel();
            cancelSemaphore.release();
        });
        cancelSemaphore.acquire();
        semaphore_.acquire();
    }
};

}  // namespace cluster::impl
