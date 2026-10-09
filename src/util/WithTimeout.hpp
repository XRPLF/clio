#pragma once

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/errc.hpp>

#include <chrono>
#include <ctime>
#include <memory>

namespace util {

/**
 * @brief Perform a coroutine operation with a timeout.
 *
 * A composed operation (e.g. a websocket write made of several socket writes) only forwards the
 * cancellation to its pending sub-operation, so a cancellation emitted in between them is lost.
 * Therefore, once the timeout expires, the cancellation is emitted again until the operation
 * completes.
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
    static constexpr auto kCancellationRetryInterval = std::chrono::milliseconds{1};

    boost::system::error_code error;
    auto operationCompleted = std::make_shared<bool>(false);
    boost::asio::cancellation_signal cancellationSignal;
    auto cyield = boost::asio::bind_cancellation_slot(cancellationSignal.slot(), yield[error]);

    boost::asio::steady_timer timer{boost::asio::get_associated_executor(cyield), timeout};
    timer.async_wait(
        [&cancellationSignal,
         &timer,
         operationCompleted](this auto const& self, boost::system::error_code errorCode) -> void {
            if (errorCode or *operationCompleted)
                return;

            cancellationSignal.emit(boost::asio::cancellation_type::terminal);
            timer.expires_after(kCancellationRetryInterval);
            timer.async_wait(self);
        }
    );
    operation(cyield);
    *operationCompleted = true;

    // Map error code to timeout
    if (error == boost::system::errc::operation_canceled) {
        return boost::system::errc::make_error_code(boost::system::errc::timed_out);
    }
    return error;
}

}  // namespace util
