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
#include <boost/asio/use_future.hpp>

#include <atomic>
#include <chrono>
#include <concepts>

namespace cluster::impl {

// TODO: Try to replace util/Repeat by this
template <typename Context>
class RepeatedTask {
    std::chrono::steady_clock::duration interval_;
    boost::asio::strand<typename Context::executor_type> strand_;

    enum class State { Running, Stopped };
    std::atomic<State> state_ = State::Stopped;

    boost::asio::cancellation_signal cancelSignal_;

public:
    RepeatedTask(std::chrono::steady_clock::duration interval, Context& ctx)
        : interval_(interval), strand_(boost::asio::make_strand(ctx))
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
        util::spawn(strand_, [this, t = std::forward<Fn>(f)](boost::asio::yield_context yield) {
            boost::asio::steady_timer timer(yield.get_executor());
            boost::system::error_code ec;
            auto token = cancelSignal_.slot();
            auto slot = boost::asio::bind_cancellation_slot(token, yield[ec]);

            while (state_ == State::Running) {
                timer.expires_after(interval_);
                timer.async_wait(slot);

                if (ec == boost::asio::error::operation_aborted or state_ != State::Running)
                    break;

                if constexpr (std::invocable<decltype(t), boost::asio::yield_context>) {
                    t(yield);
                } else {
                    t();
                }
            }
        });
    }

    void
    stop()
    {
        if (state_ == State::Stopped)
            return;

        state_ = State::Stopped;
        boost::asio::spawn(
            strand_,
            [this](auto&&) { cancelSignal_.emit(boost::asio::cancellation_type::all); },
            boost::asio::use_future
        )
            .wait();
    }
};

}  // namespace cluster::impl
