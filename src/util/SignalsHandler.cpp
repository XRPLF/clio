//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/SignalsHandler.hpp"

#include "util/Assert.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>

#include <chrono>
#include <csignal>
#include <functional>
#include <string>
#include <utility>

namespace util {

namespace impl {

class SignalsHandlerStatic {
    static SignalsHandler* handler_;

public:
    static void
    registerHandler(SignalsHandler& handler)
    {
        ASSERT(handler_ == nullptr, "There could be only one instance of SignalsHandler");
        handler_ = &handler;
    }

    static void
    handleSignal(int signal)
    {
        ASSERT(handler_ != nullptr, "SignalsHandler is not initialized");
        handler_->stopHandler_(signal);
    }

    static void
    handleSecondSignal(int signal)
    {
        ASSERT(handler_ != nullptr, "SignalsHandler is not initialized");
        handler_->secondSignalHandler_(signal);
    }
};

SignalsHandler* SignalsHandlerStatic::handler_ = nullptr;

}  // namespace impl

SignalsHandler::SignalsHandler(Config const& config, std::function<void(std::string)> forceExitHandler)
    : workGuard_(boost::asio::make_work_guard(ioContext_))
    , gracefulPeriod_(0)
    , timer_(ioContext_, gracefulPeriod_)
    , timerThread_([this] { ioContext_.run(); })
    , stopHandler_([this, forceExitHandler](int) mutable {
        LOG(LogService::info()) << "Got stop signal. Stopping Clio. Graceful period is "
                                << std::chrono::duration_cast<std::chrono::milliseconds>(gracefulPeriod_).count()
                                << " milliseconds.";
        timer_.async_wait([forceExitHandler = std::move(forceExitHandler)](const boost::system::error_code& ec) {
            if (ec != boost::asio::error::operation_aborted) {
                forceExitHandler("Force exit at the end of graceful period.");
            }
        });
        stopSignal_();
        setHandler(impl::SignalsHandlerStatic::handleSecondSignal);
    })
    , secondSignalHandler_([forceExitHandler = std::move(forceExitHandler)](int) {
        LOG(LogService::info()) << "Got second signal. Stopping Clio immediately.";
        forceExitHandler("Force exit on second signal.");
        setHandler();
    })
{
    auto const gracefulPeriod = config.valueOr("graceful_period", 10.f);
    ASSERT(gracefulPeriod >= 0.f, "Graceful period must be non-negative");
    gracefulPeriod_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<float>(gracefulPeriod));

    setHandler(impl::SignalsHandlerStatic::handleSignal);
}

SignalsHandler::~SignalsHandler()
{
    timer_.cancel();
    setHandler();
    workGuard_.reset();
    timerThread_.join();
}

void
SignalsHandler::setHandler(void (*handler)(int))
{
    for (int const signal : HANDLED_SIGNALS) {
        std::signal(signal, handler == nullptr ? SIG_DFL : handler);
    }
}

}  // namespace util
