#pragma once

#include "util/ScopeGuard.hpp"

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/errc.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <memory>
#include <utility>

namespace util {

/**
 * @brief Perform a coroutine operation with a timeout.
 *
 * A composed operation (e.g. a websocket write made of several socket writes) only forwards the
 * cancellation to its pending sub-operation, so a cancellation emitted in between them is lost.
 * Therefore, once the timeout expires, the cancellation is emitted again until the operation
 * completes, backing off in case the operation ignores it.
 *
 * @tparam Operation The operation type to perform. Must be a callable accepting yield context with
 * bound cancellation token.
 * @param operation The operation to perform.
 * @param yield The yield context.
 * @param timeout The timeout duration.
 * @return The error code of the operation.
 */
template <typename Operation>
boost::system::error_code
withTimeout(
    Operation&& operation,
    boost::asio::yield_context yield,
    std::chrono::steady_clock::duration timeout
)
{
    static constexpr auto kInitialRetryInterval = std::chrono::milliseconds{1};
    static constexpr auto kMaxRetryInterval = std::chrono::seconds{1};

    boost::system::error_code error;
    auto operationCompleted = std::make_shared<bool>(false);
    // Stops the timer handler from using this frame, even if the operation throws
    ScopeGuard const completionGuard{[operationCompleted] { *operationCompleted = true; }};
    boost::asio::cancellation_signal cancellationSignal;
    auto cyield = boost::asio::bind_cancellation_slot(cancellationSignal.slot(), yield[error]);

    boost::asio::steady_timer timer{boost::asio::get_associated_executor(cyield), timeout};
    timer.async_wait(
        [&cancellationSignal,
         &timer,
         operationCompleted,
         retryInterval = std::chrono::steady_clock::duration{kInitialRetryInterval}](
            this auto&& self, boost::system::error_code errorCode
        ) -> void {
            if (errorCode or *operationCompleted)
                return;

            cancellationSignal.emit(boost::asio::cancellation_type::terminal);
            timer.expires_after(retryInterval);
            retryInterval =
                std::min<std::chrono::steady_clock::duration>(retryInterval * 2, kMaxRetryInterval);
            timer.async_wait(std::forward<decltype(self)>(self));
        }
    );
    operation(cyield);

    // Map error code to timeout
    if (error == boost::system::errc::operation_canceled) {
        return boost::system::errc::make_error_code(boost::system::errc::timed_out);
    }
    return error;
}

}  // namespace util
