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

#pragma once

#include "util/config/ConfigDefinition.hpp"
#include "util/log/Logger.hpp"

#include <boost/signals2/signal.hpp>

#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <thread>

namespace util {
namespace impl {

class SignalsHandlerStatic;

}  // namespace impl

/**
 * @brief Class handling signals.
 * @note There could be only one instance of this class.
 */
class SignalsHandler {
    /**
     * @brief States of the signal handler state machine.
     */
    enum class State { WaitingForSignal, GracefulShutdown, ForceExit, NormalExit };

    std::chrono::steady_clock::duration gracefulPeriod_;
    std::function<void()> forceExitHandler_;

    boost::signals2::signal<void()> stopSignal_;

    std::atomic<bool> signalReceived_{false};
    std::atomic<State> state_{State::WaitingForSignal};

    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread workerThread_;

    friend class impl::SignalsHandlerStatic;

public:
    /**
     * @brief Enum for stop priority.
     */
    enum class Priority { StopFirst = 0, Normal = 1, StopLast = 2 };

    /**
     * @brief Create SignalsHandler object.
     *
     * @param config The configuration.
     * @param forceExitHandler The handler for forced exit.
     */
    SignalsHandler(
        util::config::ClioConfigDefinition const& config,
        std::function<void()> forceExitHandler = kDEFAULT_FORCE_EXIT_HANDLER
    );

    SignalsHandler(SignalsHandler const&) = delete;
    SignalsHandler(SignalsHandler&&) = delete;
    SignalsHandler&
    operator=(SignalsHandler const&) = delete;
    SignalsHandler&
    operator=(SignalsHandler&&) = delete;

    /**
     * @brief Destructor of SignalsHandler.
     */
    ~SignalsHandler();

    /**
     * @brief Subscribe to stop signal.
     *
     * @tparam SomeCallback The type of the callback.
     * @param callback The callback to call on stop signal.
     * @param priority The priority of the callback. Default is Normal.
     */
    template <std::invocable SomeCallback>
    void
    subscribeToStop(SomeCallback&& callback, Priority priority = Priority::Normal)
    {
        stopSignal_.connect(static_cast<int>(priority), std::forward<SomeCallback>(callback));
    }

    /**
     * @brief Notify the signal handler that graceful shutdown has completed.
     * This allows the handler to transition to NormalExit state.
     */
    void
    notifyGracefulShutdownComplete();

    static constexpr auto kHANDLED_SIGNALS = {SIGINT, SIGTERM};

private:
    /**
     * @brief Set signal handler for handled signals.
     *
     * @param handler The handler to set. Default is nullptr.
     */
    static void
    setHandler(void (*handler)(int) = nullptr);

    /**
     * @brief Run the state machine loop in a worker thread.
     */
    void
    runStateMachine();

    static constexpr auto kDEFAULT_FORCE_EXIT_HANDLER = []() { std::exit(EXIT_FAILURE); };
};

}  // namespace util
