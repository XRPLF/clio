//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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
#include "util/config/Config.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>
#include <sys/signal.h>

#include <chrono>
#include <csignal>
#include <functional>
#include <thread>

namespace util {

class SignalsHandler {
    boost::asio::steady_timer timer_;
    boost::asio::io_context ioContext_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> workGuard_ =
        boost::asio::make_work_guard(ioContext_);
    std::thread timerThread_;

    boost::signals2::signal<void()> stopSignal_;
    std::function<void(int)> stopHandler_ = [this](int) {
        stopSignal_();
        timer_.async_wait([this](auto) { ioContext_.stop(); });
        resetHandler();
    };

public:
    enum class Priority { StopFirst = 0, Normal = 1, StopLast = 2 };
    SignalsHandler(Config const& config)
        : timer_(ioContext_, std::chrono::seconds{config.valueOr("graceful_period", 10)})
    {
        auto callablePtr = stopHandler_.target<void(int)>();
        ASSERT(callablePtr != nullptr, "Can't get callable pointer");
        std::signal(SIGINT, callablePtr);
        std::signal(SIGHUP, callablePtr);
    }

    ~SignalsHandler()
    {
        resetHandler();
        workGuard_.reset();
        timerThread_.join();
    }

    template <typename SomeCallback>
    void
    subscribeToStop(SomeCallback&& callback, Priority priority = Priority::Normal)
    {
        stopSignal_.connect(static_cast<int>(priority), std::forward<SomeCallback>(callback));
    }

private:
    static void
    resetHandler()
    {
        std::signal(SIGINT, SIG_DFL);
        std::signal(SIGINT, SIG_DFL);
    }
};

}  // namespace util
