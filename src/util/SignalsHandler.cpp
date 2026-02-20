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
#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

namespace util {
namespace impl {

class SignalsHandlerStatic {
    static SignalsHandler* installedHandler;

public:
    static void
    registerHandler(SignalsHandler& handler)
    {
        ASSERT(installedHandler == nullptr, "There could be only one instance of SignalsHandler");
        installedHandler = &handler;
    }

    static void
    resetHandler()
    {
        installedHandler = nullptr;
    }

    static void
    handleSignal(int /* signal */)
    {
        ASSERT(installedHandler != nullptr, "SignalsHandler is not initialized");
        installedHandler->signalReceived_ = true;
        installedHandler->cv_.notify_one();
    }
};

SignalsHandler* SignalsHandlerStatic::installedHandler = nullptr;

}  // namespace impl

SignalsHandler::SignalsHandler(
    config::ClioConfigDefinition const& config,
    std::function<void()> forceExitHandler
)
    : gracefulPeriod_(
          util::config::ClioConfigDefinition::toMilliseconds(config.get<float>("graceful_period"))
      )
    , forceExitHandler_(std::move(forceExitHandler))
{
    impl::SignalsHandlerStatic::registerHandler(*this);
    workerThread_ = std::thread([this]() { runStateMachine(); });
    setHandler(impl::SignalsHandlerStatic::handleSignal);
}

SignalsHandler::~SignalsHandler()
{
    setHandler();

    state_ = State::NormalExit;
    cv_.notify_one();

    if (workerThread_.joinable())
        workerThread_.join();

    impl::SignalsHandlerStatic::resetHandler();  // This is needed mostly for tests to reset static
                                                 // state
}

void
SignalsHandler::notifyGracefulShutdownComplete()
{
    if (state_ == State::GracefulShutdown) {
        LOG(LogService::info()) << "Graceful shutdown completed successfully.";
        state_ = State::NormalExit;
        cv_.notify_one();
    }
}

void
SignalsHandler::setHandler(void (*handler)(int))
{
    for (int const signal : kHANDLED_SIGNALS)
        std::signal(signal, handler == nullptr ? SIG_DFL : handler);
}

void
SignalsHandler::runStateMachine()
{
    while (state_ != State::NormalExit) {
        auto currentState = state_.load();

        switch (currentState) {
            case State::WaitingForSignal: {
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cv_.wait(lock, [this]() {
                        return signalReceived_ or state_ == State::NormalExit;
                    });
                }

                if (state_ == State::NormalExit)
                    return;

                LOG(
                    LogService::info()
                ) << "Got stop signal. Stopping Clio. Graceful period is "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(gracefulPeriod_).count()
                  << " milliseconds.";

                state_ = State::GracefulShutdown;
                signalReceived_ = false;

                stopSignal_();
                break;
            }

            case State::GracefulShutdown: {
                bool waitResult = false;
                {
                    std::unique_lock<std::mutex> lock(mutex_);

                    // Wait for either:
                    // 1. Graceful period to elapse (timeout)
                    // 2. Another signal (signalReceived_)
                    // 3. Graceful shutdown completion (state changes to NormalExit)
                    waitResult = cv_.wait_for(lock, gracefulPeriod_, [this]() {
                        return signalReceived_ or state_ == State::NormalExit;
                    });
                }

                if (state_ == State::NormalExit)
                    break;

                if (signalReceived_) {
                    LOG(LogService::warn()) << "Force exit on second signal.";
                    state_ = State::ForceExit;
                    signalReceived_ = false;
                } else if (not waitResult) {
                    LOG(LogService::warn()) << "Force exit at the end of graceful period.";
                    state_ = State::ForceExit;
                }
                break;
            }

            case State::ForceExit: {
                forceExitHandler_();
                state_ = State::NormalExit;
                break;
            }

            case State::NormalExit:
                return;
        }
    }
}

}  // namespace util
