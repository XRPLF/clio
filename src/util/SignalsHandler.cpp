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
#include "util/Constants.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"

#include <chrono>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <functional>
#include <optional>
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
    resetHandler()
    {
        handler_ = nullptr;
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

SignalsHandler::SignalsHandler(Config const& config, std::function<void()> forceExitHandler)
    : gracefulPeriod_(0)
    , context_(1)
    , stopHandler_([this, forceExitHandler](int) mutable {
        LOG(LogService::info()) << "Got stop signal. Stopping Clio. Graceful period is "
                                << std::chrono::duration_cast<std::chrono::milliseconds>(gracefulPeriod_).count()
                                << " milliseconds.";
        setHandler(impl::SignalsHandlerStatic::handleSecondSignal);
        timer_.emplace(context_.scheduleAfter(
            gracefulPeriod_,
            [forceExitHandler = std::move(forceExitHandler)](auto&& stopToken, bool canceled) {
                // TODO: Update this after https://github.com/XRPLF/clio/issues/1380
                if (not stopToken.isStopRequested() and not canceled) {
                    LOG(LogService::warn()) << "Force exit at the end of graceful period.";
                    forceExitHandler();
                }
            }
        ));
        stopSignal_();
    })
    , secondSignalHandler_([this, forceExitHandler = std::move(forceExitHandler)](int) {
        LOG(LogService::warn()) << "Force exit on second signal.";
        forceExitHandler();
        cancelTimer();
        setHandler();
    })
{
    impl::SignalsHandlerStatic::registerHandler(*this);

    auto const gracefulPeriod =
        std::round(config.valueOr("graceful_period", 10.f) * static_cast<float>(util::MILLISECONDS_PER_SECOND));
    ASSERT(gracefulPeriod >= 0.f, "Graceful period must be non-negative");
    gracefulPeriod_ = std::chrono::milliseconds{static_cast<size_t>(gracefulPeriod)};

    setHandler(impl::SignalsHandlerStatic::handleSignal);
}

SignalsHandler::~SignalsHandler()
{
    cancelTimer();
    setHandler();
    impl::SignalsHandlerStatic::resetHandler();  // This is needed mostly for tests to reset static state
}

void
SignalsHandler::cancelTimer()
{
    if (timer_.has_value())
        timer_->abort();
}

void
SignalsHandler::setHandler(void (*handler)(int))
{
    for (int const signal : HANDLED_SIGNALS) {
        std::signal(signal, handler == nullptr ? SIG_DFL : handler);
    }
}

}  // namespace util
