#include "util/WithTimeout.hpp"

#include "util/AsioContextTestFixture.hpp"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/detail/error_code.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <utility>

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

TEST_F(WithTimeoutTests, TimeoutBetweenSubOperationsIsNotLost)
{
    // Like a websocket write made of several socket writes, this operation only lets its pending
    // sub-operation be cancelled. The timeout fires while the first, uncancellable sub-operation
    // is pending and must still cancel the second one.
    boost::asio::steady_timer timer{ctx_};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const error = util::withTimeout(
            [&timer](auto cyield) {
                boost::asio::async_compose<decltype(cyield), void(boost::system::error_code)>(
                    [&timer, step = 0](auto& self, boost::system::error_code error = {}) mutable {
                        switch (step++) {
                            case 0:
                                timer.expires_after(std::chrono::milliseconds{10});
                                timer.async_wait(
                                    boost::asio::bind_cancellation_slot(
                                        boost::asio::cancellation_slot{}, std::move(self)
                                    )
                                );
                                return;
                            case 1:
                                timer.expires_after(std::chrono::seconds{1});
                                timer.async_wait(std::move(self));
                                return;
                            default:
                                self.complete(error);
                        }
                    },
                    cyield,
                    timer
                );
            },
            yield,
            std::chrono::milliseconds{1}
        );
        EXPECT_EQ(error.value(), boost::system::errc::timed_out);
    });
}

TEST_F(WithTimeoutTests, OperationIgnoringCancellationCompletes)
{
    boost::asio::steady_timer timer{ctx_};
    runSpawn([&](boost::asio::yield_context yield) {
        auto const error = util::withTimeout(
            [&timer](auto cyield) {
                timer.expires_after(std::chrono::milliseconds{20});
                timer.async_wait(
                    boost::asio::bind_cancellation_slot(boost::asio::cancellation_slot{}, cyield)
                );
            },
            yield,
            std::chrono::milliseconds{1}
        );
        EXPECT_EQ(error, boost::system::error_code{});
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
