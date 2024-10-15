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

#include "util/AsioContextTestFixture.hpp"
#include "util/Taggable.hpp"
#include "util/UnsupportedType.hpp"
#include "util/config/Config.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/MockConnection.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/ConnectionHandler.hpp"

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>

using namespace web::ng::impl;
using namespace web::ng;
using testing::Return;
namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;

struct ConnectionHandlerTest : SyncAsioContextTest {
    ConnectionHandlerTest(ConnectionHandler::ProcessingPolicy policy, std::optional<size_t> maxParallelConnections)
        : connectionHandler_{policy, maxParallelConnections}
    {
    }

    template <typename BoostErrorType>
    static std::unexpected<Error>
    makeError(BoostErrorType error)
    {
        if constexpr (std::same_as<BoostErrorType, http::error>) {
            return std::unexpected{http::make_error_code(error)};
        } else if constexpr (std::same_as<BoostErrorType, websocket::error>) {
            return std::unexpected{websocket::make_error_code(error)};
        } else if constexpr (std::same_as<BoostErrorType, boost::asio::error::basic_errors> ||
                             std::same_as<BoostErrorType, boost::asio::error::misc_errors> ||
                             std::same_as<BoostErrorType, boost::asio::error::addrinfo_errors> ||
                             std::same_as<BoostErrorType, boost::asio::error::netdb_errors>) {
            return std::unexpected{boost::asio::error::make_error_code(error)};
        } else {
            static_assert(util::Unsupported<BoostErrorType>, "Wrong error type");
        }
    }

    template <typename... Args>
    static std::expected<Request, Error>
    makeRequest(Args&&... args)
    {
        return Request{std::forward<Args>(args)...};
    }

    ConnectionHandler connectionHandler_;

    util::TagDecoratorFactory tagDecoratorFactory_{util::Config(boost::json::object{{"log_tag_style", "uint"}})};
    StrictMockConnectionPtr mockConnection_ =
        std::make_unique<StrictMockConnection>("1.2.3.4", beast::flat_buffer{}, tagDecoratorFactory_);
};

struct ConnectionHandlerSequentialProcessingTest : ConnectionHandlerTest {
    ConnectionHandlerSequentialProcessingTest()
        : ConnectionHandlerTest(ConnectionHandler::ProcessingPolicy::Sequential, std::nullopt)
    {
    }
};

TEST_F(ConnectionHandlerSequentialProcessingTest, ReceiveError)
{
    EXPECT_CALL(*mockConnection_, receive).WillOnce(Return(makeError(http::error::end_of_stream)));

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, ReceiveError_CloseConnection)
{
    EXPECT_CALL(*mockConnection_, receive).WillOnce(Return(makeError(boost::asio::error::timed_out)));
    EXPECT_CALL(*mockConnection_, close);

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_NoHandler_Send)
{
    EXPECT_CALL(*mockConnection_, receive)
        .WillOnce(Return(makeRequest("some_request", Request::HttpHeaders{})))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(*mockConnection_, send).WillOnce([](Response response, auto&&, auto&&) {
        EXPECT_EQ(response.message(), "WebSocket is not supported by this server");
        return std::nullopt;
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_BadTarget_Send)
{
    std::string const target = "/some/target";

    std::string const requestMessage = "some message";
    EXPECT_CALL(*mockConnection_, receive)
        .WillOnce(Return(makeRequest(http::request<http::string_body>{http::verb::get, target, 11, requestMessage})))
        .WillOnce(Return(makeError(http::error::end_of_stream)));

    EXPECT_CALL(*mockConnection_, send).WillOnce([](Response response, auto&&, auto&&) {
        EXPECT_EQ(response.message(), "Bad target");
        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), http::status::bad_request);
        EXPECT_EQ(httpResponse.version(), 11);
        return std::nullopt;
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_Send)
{
    testing::StrictMock<testing::MockFunction<Response(Request const&, ConnectionContext, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler_.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";
    EXPECT_CALL(*mockConnection_, receive)
        .WillOnce(Return(makeRequest(requestMessage, Request::HttpHeaders{})))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call).WillOnce([&](Request const& request, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockConnection_, send).WillOnce([&responseMessage](Response response, auto&&, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return std::nullopt;
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_Send_Loop)
{
    std::string const target = "/some/target";
    testing::StrictMock<testing::MockFunction<Response(Request const&, ConnectionContext, boost::asio::yield_context)>>
        postHandlerMock;
    connectionHandler_.onPost(target, postHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    auto const returnRequest =
        Return(makeRequest(http::request<http::string_body>{http::verb::post, target, 11, requestMessage}));
    EXPECT_CALL(*mockConnection_, receive)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(Return(makeError(http::error::partial_message)));

    EXPECT_CALL(postHandlerMock, Call).Times(3).WillRepeatedly([&](Request const& request, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockConnection_, send).Times(3).WillRepeatedly([&responseMessage](Response response, auto&&, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return std::nullopt;
    });

    EXPECT_CALL(*mockConnection_, close);

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_SendError)
{
    std::string const target = "/some/target";
    testing::StrictMock<testing::MockFunction<Response(Request const&, ConnectionContext, boost::asio::yield_context)>>
        getHandlerMock;

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    connectionHandler_.onGet(target, getHandlerMock.AsStdFunction());

    EXPECT_CALL(*mockConnection_, receive)
        .WillOnce(Return(makeRequest(http::request<http::string_body>{http::verb::get, target, 11, requestMessage})));

    EXPECT_CALL(getHandlerMock, Call).WillOnce([&](Request const& request, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockConnection_, send).WillOnce([&responseMessage](Response response, auto&&, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return makeError(http::error::end_of_stream).error();
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Stop)
{
    testing::StrictMock<testing::MockFunction<Response(Request const&, ConnectionContext, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler_.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";
    bool connectionClosed = false;
    EXPECT_CALL(*mockConnection_, receive)
        .Times(4)
        .WillRepeatedly([&](auto&&, auto&&) -> std::expected<Request, Error> {
            if (connectionClosed) {
                return makeError(websocket::error::closed);
            }
            return makeRequest(requestMessage, Request::HttpHeaders{});
        });

    EXPECT_CALL(wsHandlerMock, Call).Times(3).WillRepeatedly([&](Request const& request, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    size_t numCalls = 0;
    EXPECT_CALL(*mockConnection_, send).Times(3).WillRepeatedly([&](Response response, auto&&, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);

        ++numCalls;
        if (numCalls == 3)
            connectionHandler_.stop();

        return std::nullopt;
    });

    EXPECT_CALL(*mockConnection_, close).WillOnce([&connectionClosed]() { connectionClosed = true; });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

struct ConnectionHandlerParallelProcessingTest : ConnectionHandlerTest {
    static size_t const maxParallelRequests = 3;
    ConnectionHandlerParallelProcessingTest()
        : ConnectionHandlerTest(ConnectionHandler::ProcessingPolicy::Parallel, maxParallelRequests)
    {
    }

    static void
    asyncSleep(boost::asio::yield_context yield, std::chrono::steady_clock::duration duration)
    {
        boost::asio::steady_timer timer{yield.get_executor()};
        std::cout << "sleep starts" << std::endl;
        timer.expires_after(duration);
        timer.async_wait(yield);
        std::cout << "sleep ends" << std::endl;
    }
};

TEST_F(ConnectionHandlerParallelProcessingTest, ReceiveError)
{
    EXPECT_CALL(*mockConnection_, receive).WillOnce(Return(makeError(http::error::end_of_stream)));

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, Receive_Handle_Send)
{
    testing::StrictMock<testing::MockFunction<Response(Request const&, ConnectionContext, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler_.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";
    EXPECT_CALL(*mockConnection_, receive)
        .WillOnce(Return(makeRequest(requestMessage, Request::HttpHeaders{})))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call).WillOnce([&](Request const& request, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockConnection_, send).WillOnce([&responseMessage](Response response, auto&&, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return std::nullopt;
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, Receive_Handle_Send_Loop)
{
    testing::StrictMock<testing::MockFunction<Response(Request const&, ConnectionContext, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler_.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    auto const returnRequest = [&](auto&&, auto&&) { return makeRequest(requestMessage, Request::HttpHeaders{}); };
    EXPECT_CALL(*mockConnection_, receive)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call).Times(2).WillRepeatedly([&](Request const& request, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockConnection_, send).Times(2).WillRepeatedly([&responseMessage](Response response, auto&&, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return std::nullopt;
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, Receive_Handle_Send_Loop_TooManyRequest)
{
    testing::StrictMock<testing::MockFunction<Response(Request const&, ConnectionContext, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler_.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    auto const returnRequest = [&](auto&&, auto&&) { return makeRequest(requestMessage, Request::HttpHeaders{}); };
    testing::Sequence sequence;
    EXPECT_CALL(*mockConnection_, receive)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call)
        .Times(3)
        .WillRepeatedly([&](Request const& request, auto&&, boost::asio::yield_context yield) {
            EXPECT_EQ(request.message(), requestMessage);
            asyncSleep(yield, std::chrono::milliseconds{3});
            std::cout << "After sleep" << std::endl;
            return Response(http::status::ok, responseMessage, request);
        });

    EXPECT_CALL(
        *mockConnection_,
        send(
            testing::ResultOf(
                [](Response response) {
                    std::cout << "1 " << response.message() << std::endl;
                    return response.message();
                },
                responseMessage
            ),
            testing::_,
            testing::_
        )
    )
        .Times(3)
        .WillRepeatedly(Return(std::nullopt));

    EXPECT_CALL(
        *mockConnection_,
        send(
            testing::ResultOf(
                [](Response response) {
                    std::cout << "2 " << response.message() << std::endl;
                    return response.message();
                },
                "Too many requests for one session"
            ),
            testing::_,
            testing::_
        )
    )
        .Times(2)
        .WillRepeatedly(Return(std::nullopt));

    // .WillRepeatedly([&](Response response, auto&&, auto&&) {
    //     if (handlerCallCounter < 3) {
    //         EXPECT_EQ(response.message(), responseMessage);
    //         return std::nullopt;
    //     }
    //     EXPECT_EQ(response.message(), "Too many requests for one session");
    // return std::nullopt;
    // });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler_.processConnection(std::move(mockConnection_), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, myTest)
{
    runSpawn([](boost::asio::yield_context yield) {
        boost::asio::steady_timer sync{yield.get_executor(), boost::asio::steady_timer::clock_type::duration::max()};

        int childNumber = 0;
        boost::asio::spawn(yield, [&](boost::asio::yield_context innerYield) {
            ++childNumber;

            std::cout << "I'm child coroutine" << std::endl;
            boost::asio::steady_timer t{innerYield.get_executor()};
            t.expires_after(std::chrono::milliseconds{20});
            t.async_wait(innerYield);
            std::cout << "Child 1 done" << std::endl;

            --childNumber;
            if (childNumber == 0)
                sync.cancel();
        });

        std::cout << "Parent: between" << std::endl;

        boost::asio::spawn(yield, [&](auto&& innerYield) {
            ++childNumber;

            std::cout << "I'm child coroutine 2" << std::endl;
            boost::asio::steady_timer t{innerYield.get_executor()};
            t.expires_after(std::chrono::milliseconds{30});
            t.async_wait(innerYield);
            std::cout << "Child 2 done" << std::endl;

            --childNumber;
            if (childNumber == 0)
                sync.cancel();
        });

        boost::system::error_code error;
        sync.async_wait(yield[error]);
        std::cout << "Parent done" << std::endl;
    });
}
