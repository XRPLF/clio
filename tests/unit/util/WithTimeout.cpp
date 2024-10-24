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

#include "util/WithTimeout.hpp"

#include "util/AsioContextTestFixture.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

struct WithTimeoutTests : SyncAsioContextTest {
    using CYieldType = boost::asio::cancellation_slot_binder<
        boost::asio::basic_yield_context<boost::asio::any_io_executor>,
        boost::asio::cancellation_slot>;

    testing::StrictMock<testing::MockFunction<void(CYieldType)>> operationMock;
};

TEST_F(WithTimeoutTests, CallsOperation)
{
    EXPECT_CALL(operationMock, Call);
    runSpawn([&](boost::asio::yield_context yield) {
        auto const error = util::withTimeout(operationMock.AsStdFunction(), yield, std::chrono::seconds{1});
        EXPECT_EQ(error, boost::system::error_code{});
    });
}

TEST_F(WithTimeoutTests, TimesOut)
{
    EXPECT_CALL(operationMock, Call).WillOnce([](auto cyield) {
        boost::asio::steady_timer timer{boost::asio::get_associated_executor(cyield)};
        timer.expires_after(std::chrono::milliseconds{10});
        timer.async_wait(cyield);
    });
    runSpawn([&](boost::asio::yield_context yield) {
        auto error = util::withTimeout(operationMock.AsStdFunction(), yield, std::chrono::milliseconds{1});
        EXPECT_EQ(error.value(), boost::system::errc::timed_out);
    });
}

TEST_F(WithTimeoutTests, OperationFailed)
{
    EXPECT_CALL(operationMock, Call).WillOnce([](auto cyield) {
        boost::asio::ip::tcp::socket socket{boost::asio::get_associated_executor(cyield)};
        socket.async_send(boost::asio::buffer("test"), cyield);
    });
    runSpawn([&](boost::asio::yield_context yield) {
        auto error = util::withTimeout(operationMock.AsStdFunction(), yield, std::chrono::seconds{1});
        EXPECT_EQ(error.value(), boost::system::errc::bad_file_descriptor);
    });
}
