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

#include "util/async/context/BasicExecutionContext.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/variadic_signal.hpp>

#include <chrono>
#include <concepts>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <optional>

namespace util {

namespace impl {
class SignalsHandlerStatic;
}  // namespace impl

/**
 * @brief Class handling signals.
 * @note There could be only one instance of this class.
 */
class SignalsHandler {
    std::chrono::steady_clock::duration gracefulPeriod_;
    async::PoolExecutionContext context_;
    std::optional<async::PoolExecutionContext::ScheduledOperation<void>> timer_;

    boost::signals2::signal<void()> stopSignal_;
    std::function<void(int)> stopHandler_;
    std::function<void(int)> secondSignalHandler_;

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
        std::function<void()> forceExitHandler = defaultForceExitHandler_
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

    static constexpr auto HANDLED_SIGNALS = {SIGINT, SIGTERM};

private:
    /**
     * @brief Cancel scheduled force exit if any.
     */
    void
    cancelTimer();

    /**
     * @brief Set signal handler for handled signals.
     *
     * @param handler The handler to set. Default is nullptr.
     */
    static void
    setHandler(void (*handler)(int) = nullptr);

    static auto constexpr defaultForceExitHandler_ = []() { std::exit(EXIT_FAILURE); };
};

}  // namespace util
