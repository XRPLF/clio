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
    boost::system::error_code error;
    auto operationCompleted = std::make_shared<bool>(false);
    boost::asio::cancellation_signal cancellationSignal;
    auto cyield = boost::asio::bind_cancellation_slot(cancellationSignal.slot(), yield[error]);

    boost::asio::steady_timer timer{boost::asio::get_associated_executor(cyield), timeout};
    timer.async_wait([&cancellationSignal,
                      operationCompleted](boost::system::error_code errorCode) {
        if (!errorCode and !*operationCompleted)
            cancellationSignal.emit(boost::asio::cancellation_type::terminal);
    });
    operation(cyield);
    *operationCompleted = true;

    // Map error code to timeout
    if (error == boost::system::errc::operation_canceled) {
        return boost::system::errc::make_error_code(boost::system::errc::timed_out);
    }
    return error;
}

}  // namespace util
