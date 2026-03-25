#include "util/WithTimeout.hpp"

#include "util/AsioContextTestFixture.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancellation_signal.hpp>
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
        auto const error =
            util::withTimeout(operationMock.AsStdFunction(), yield, std::chrono::seconds{1});
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
        auto error =
            util::withTimeout(operationMock.AsStdFunction(), yield, std::chrono::milliseconds{1});
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
        auto error =
            util::withTimeout(operationMock.AsStdFunction(), yield, std::chrono::seconds{1});
        EXPECT_EQ(error.value(), boost::system::errc::bad_file_descriptor);
    });
}
