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

namespace util {

/** Perform a coroutine operation with a timeout.
    @param operation The operation to perform.
    @param yield The yield context.
    @param timeout The timeout duration.
    @return The error code of the operation.
*/
template <typename Operation>
boost::system::error_code
withTimeout(Operation&& operation, boost::asio::yield_context yield, std::chrono::steady_clock::duration timeout)
{
    boost::system::error_code error;
    boost::asio::cancellation_signal cancellationSignal;
    auto cyield = boost::asio::bind_cancellation_slot(cancellationSignal.slot(), yield[error]);

    boost::asio::steady_timer timer{boost::asio::get_associated_executor(cyield), timeout};
    timer.async_wait([&cancellationSignal](boost::system::error_code errorCode) {
        if (!errorCode)
            cancellationSignal.emit(boost::asio::cancellation_type::terminal);
    });
    operation(cyield);

    // Map error code to timeout
    if (error == boost::system::errc::operation_canceled) {
        return boost::system::errc::make_error_code(boost::system::errc::timed_out);
    }
    return error;
}

}  // namespace util
