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
#include "util/MockPrometheus.hpp"
#include "util/Taggable.hpp"
#include "util/UnsupportedType.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/ProcessingPolicy.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/ConnectionHandler.hpp"
#include "web/ng/impl/MockHttpConnection.hpp"
#include "web/ng/impl/MockWsConnection.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/websocket/error.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace web::ng::impl;
using namespace web::ng;
using namespace util;
using testing::Return;
namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;

struct ConnectionHandlerTest : prometheus::WithPrometheus, SyncAsioContextTest {
    ConnectionHandlerTest(ProcessingPolicy policy, std::optional<size_t> maxParallelConnections)
        : tagFactory{util::config::ClioConfigDefinition{
              {"log_tag_style", config::ConfigValue{config::ConfigType::String}.defaultValue("uint")}
          }}
        , connectionHandler{policy, maxParallelConnections, tagFactory, std::nullopt, onDisconnectMock.AsStdFunction()}
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

    testing::StrictMock<testing::MockFunction<void(Connection const&)>> onDisconnectMock;
    util::TagDecoratorFactory tagFactory;
    ConnectionHandler connectionHandler;

    util::TagDecoratorFactory tagDecoratorFactory{config::ClioConfigDefinition{
        {"log_tag_style", config::ConfigValue{config::ConfigType::String}.defaultValue("uint")}
    }};
    StrictMockHttpConnectionPtr mockHttpConnection =
        std::make_unique<StrictMockHttpConnection>("1.2.3.4", beast::flat_buffer{}, tagDecoratorFactory);
    StrictMockWsConnectionPtr mockWsConnection =
        std::make_unique<StrictMockWsConnection>("1.2.3.4", beast::flat_buffer{}, tagDecoratorFactory);

    Request::HttpHeaders headers;
};

struct ConnectionHandlerSequentialProcessingTest : ConnectionHandlerTest {
    ConnectionHandlerSequentialProcessingTest() : ConnectionHandlerTest(ProcessingPolicy::Sequential, std::nullopt)
    {
    }
};

TEST_F(ConnectionHandlerSequentialProcessingTest, ReceiveError)
{
    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive).WillOnce(Return(makeError(http::error::end_of_stream)));
    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, ReceiveError_CloseConnection)
{
    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive).WillOnce(Return(makeError(boost::asio::error::timed_out)));
    EXPECT_CALL(
        *mockHttpConnection,
        setTimeout(std::chrono::steady_clock::duration{ConnectionHandler::kCLOSE_CONNECTION_TIMEOUT})
    );
    EXPECT_CALL(*mockHttpConnection, close);
    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_NoHandler_Send)
{
    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(Return(makeRequest("some_request", headers)))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(*mockHttpConnection, send).WillOnce([](Response response, auto&&) {
        EXPECT_EQ(response.message(), "WebSocket is not supported by this server");
        return std::nullopt;
    });

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_BadTarget_Send)
{
    std::string const target = "/some/target";
    std::string const requestMessage = "some message";

    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(Return(makeRequest(http::request<http::string_body>{http::verb::get, target, 11, requestMessage})))
        .WillOnce(Return(makeError(http::error::end_of_stream)));

    EXPECT_CALL(*mockHttpConnection, send).WillOnce([](Response response, auto&&) {
        EXPECT_EQ(response.message(), "Bad target");
        auto const httpResponse = std::move(response).intoHttpResponse();
        EXPECT_EQ(httpResponse.result(), http::status::bad_request);
        EXPECT_EQ(httpResponse.version(), 11);
        return std::nullopt;
    });

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_BadMethod_Send)
{
    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(Return(makeRequest(http::request<http::string_body>{http::verb::acl, "/", 11})))
        .WillOnce(Return(makeError(http::error::end_of_stream)));

    EXPECT_CALL(*mockHttpConnection, send).WillOnce([](Response response, auto&&) {
        EXPECT_EQ(response.message(), "Unsupported http method");
        return std::nullopt;
    });

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_Send)
{
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    EXPECT_CALL(*mockWsConnection, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(*mockWsConnection, receive)
        .WillOnce(Return(makeRequest(requestMessage, headers)))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call).WillOnce([&](Request const& request, auto&&, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockWsConnection, send).WillOnce([&responseMessage](Response response, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return std::nullopt;
    });

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, SendSubscriptionMessage)
{
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    std::string const subscriptionMessage = "subscription message";

    EXPECT_CALL(*mockWsConnection, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(*mockWsConnection, receive)
        .WillOnce(Return(makeRequest("", headers)))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call)
        .WillOnce([&](Request const& request, auto&&, web::SubscriptionContextPtr subscriptionContext, auto&&) {
            EXPECT_NE(subscriptionContext, nullptr);
            subscriptionContext->send(std::make_shared<std::string>(subscriptionMessage));
            return Response(http::status::ok, "", request);
        });

    EXPECT_CALL(*mockWsConnection, send).WillOnce(Return(std::nullopt));

    EXPECT_CALL(*mockWsConnection, sendBuffer)
        .WillOnce([&subscriptionMessage](boost::asio::const_buffer buffer, auto&&) {
            EXPECT_EQ(boost::beast::buffers_to_string(buffer), subscriptionMessage);
            return std::nullopt;
        });

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, SubscriptionContextIsDisconnectedAfterProcessingFinished)
{
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    testing::StrictMock<testing::MockFunction<void(web::SubscriptionContextInterface*)>> onDisconnectHook;

    EXPECT_CALL(*mockWsConnection, wasUpgraded).WillOnce(Return(true));
    testing::Expectation const expectationReceiveCalled = EXPECT_CALL(*mockWsConnection, receive)
                                                              .WillOnce(Return(makeRequest("", headers)))
                                                              .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call)
        .WillOnce([&](Request const& request, auto&&, web::SubscriptionContextPtr subscriptionContext, auto&&) {
            EXPECT_NE(subscriptionContext, nullptr);
            subscriptionContext->onDisconnect(onDisconnectHook.AsStdFunction());
            return Response(http::status::ok, "", request);
        });

    EXPECT_CALL(*mockWsConnection, send).WillOnce(Return(std::nullopt));

    EXPECT_CALL(onDisconnectHook, Call).After(expectationReceiveCalled);

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, SubscriptionContextIsNullForHttpConnection)
{
    std::string const target = "/some/target";
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        postHandlerMock;
    connectionHandler.onPost(target, postHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    auto const returnRequest =
        Return(makeRequest(http::request<http::string_body>{http::verb::post, target, 11, requestMessage}));

    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(returnRequest)
        .WillOnce(Return(makeError(http::error::partial_message)));

    EXPECT_CALL(postHandlerMock, Call)
        .WillOnce([&](Request const& request, auto&&, web::SubscriptionContextPtr subscriptionContext, auto&&) {
            EXPECT_EQ(subscriptionContext, nullptr);

            return Response(http::status::ok, responseMessage, request);
        });

    EXPECT_CALL(*mockHttpConnection, send).WillOnce([&responseMessage](Response response, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return std::nullopt;
    });

    EXPECT_CALL(
        *mockHttpConnection,
        setTimeout(std::chrono::steady_clock::duration{ConnectionHandler::kCLOSE_CONNECTION_TIMEOUT})
    );
    EXPECT_CALL(*mockHttpConnection, close);

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_Send_Loop)
{
    std::string const target = "/some/target";
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        postHandlerMock;
    connectionHandler.onPost(target, postHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    auto const returnRequest =
        Return(makeRequest(http::request<http::string_body>{http::verb::post, target, 11, requestMessage}));

    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(Return(makeError(http::error::partial_message)));

    EXPECT_CALL(postHandlerMock, Call).Times(3).WillRepeatedly([&](Request const& request, auto&&, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockHttpConnection, send).Times(3).WillRepeatedly([&responseMessage](Response response, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return std::nullopt;
    });

    EXPECT_CALL(
        *mockHttpConnection,
        setTimeout(std::chrono::steady_clock::duration{ConnectionHandler::kCLOSE_CONNECTION_TIMEOUT})
    );
    EXPECT_CALL(*mockHttpConnection, close);

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_SendError)
{
    std::string const target = "/some/target";
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        getHandlerMock;

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    connectionHandler.onGet(target, getHandlerMock.AsStdFunction());

    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(Return(makeRequest(http::request<http::string_body>{http::verb::get, target, 11, requestMessage})));

    EXPECT_CALL(getHandlerMock, Call).WillOnce([&](Request const& request, auto&&, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockHttpConnection, send).WillOnce([&responseMessage](Response response, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return makeError(http::error::end_of_stream).error();
    });

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Stop)
{
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";
    bool connectionClosed = false;

    EXPECT_CALL(*mockWsConnection, wasUpgraded).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(*mockWsConnection, receive).Times(4).WillRepeatedly([&](auto&&) -> std::expected<Request, Error> {
        if (connectionClosed) {
            return makeError(websocket::error::closed);
        }
        return makeRequest(requestMessage, headers);
    });

    EXPECT_CALL(wsHandlerMock, Call).Times(3).WillRepeatedly([&](Request const& request, auto&&, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    size_t numCalls = 0;
    EXPECT_CALL(
        *mockWsConnection,
        send(testing::ResultOf([](Response const& r) { return r.message(); }, responseMessage), testing::_)
    )
        .Times(3)
        .WillRepeatedly([&](auto&&, auto&&) {
            ++numCalls;
            if (numCalls == 3)
                boost::asio::spawn(ctx_, [this](auto yield) { connectionHandler.stop(yield); });

            return std::nullopt;
        });

    EXPECT_CALL(
        *mockWsConnection,
        send(
            testing::ResultOf(
                [](Response const& r) { return r.message(); },
                "This Clio node is shutting down. Please try another node."
            ),
            testing::_
        )
    );

    EXPECT_CALL(
        *mockWsConnection, setTimeout(std::chrono::steady_clock::duration{ConnectionHandler::kCLOSE_CONNECTION_TIMEOUT})
    );
    EXPECT_CALL(*mockWsConnection, close).WillOnce([&connectionClosed]() { connectionClosed = true; });

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, ProcessCalledAfterStop)
{
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    runSyncOperation([this](boost::asio::yield_context yield) { connectionHandler.stop(yield); });

    EXPECT_CALL(*mockWsConnection, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(
        *mockWsConnection,
        send(
            testing::ResultOf(
                [](Response const& r) { return r.message(); }, testing::HasSubstr("This Clio node is shutting down")
            ),
            testing::_
        )
    );

    EXPECT_CALL(
        *mockWsConnection, setTimeout(std::chrono::steady_clock::duration{ConnectionHandler::kCLOSE_CONNECTION_TIMEOUT})
    );
    EXPECT_CALL(*mockWsConnection, close);

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

struct ConnectionHandlerParallelProcessingTest : ConnectionHandlerTest {
    static constexpr size_t kMAX_PARALLEL_REQUESTS = 3;

    ConnectionHandlerParallelProcessingTest()
        : ConnectionHandlerTest(
              ProcessingPolicy::Parallel,
              ConnectionHandlerParallelProcessingTest::kMAX_PARALLEL_REQUESTS
          )
    {
    }

    static void
    asyncSleep(boost::asio::yield_context yield, std::chrono::steady_clock::duration duration)
    {
        boost::asio::steady_timer timer{yield.get_executor()};
        timer.expires_after(duration);
        timer.async_wait(yield);
    }
};

TEST_F(ConnectionHandlerParallelProcessingTest, ReceiveError)
{
    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive).WillOnce(Return(makeError(http::error::end_of_stream)));

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, Receive_Handle_Send)
{
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    EXPECT_CALL(*mockWsConnection, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(*mockWsConnection, receive)
        .WillOnce(Return(makeRequest(requestMessage, headers)))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call).WillOnce([&](Request const& request, auto&&, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockWsConnection, send).WillOnce([&responseMessage](Response response, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return std::nullopt;
    });

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, Receive_Handle_Send_Loop)
{
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    auto const returnRequest = [&](auto&&) { return makeRequest(requestMessage, headers); };

    EXPECT_CALL(*mockWsConnection, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(*mockWsConnection, receive)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call).Times(2).WillRepeatedly([&](Request const& request, auto&&, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockWsConnection, send).Times(2).WillRepeatedly([&responseMessage](Response response, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return std::nullopt;
    });

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, Receive_Handle_Send_Loop_TooManyRequest)
{
    testing::StrictMock<testing::MockFunction<
        Response(Request const&, ConnectionMetadata const&, web::SubscriptionContextPtr, boost::asio::yield_context)>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    auto const returnRequest = [&](auto&&) { return makeRequest(requestMessage, headers); };
    testing::Sequence const sequence;

    EXPECT_CALL(*mockWsConnection, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(*mockWsConnection, receive)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call)
        .Times(3)
        .WillRepeatedly([&](Request const& request, auto&&, auto&&, boost::asio::yield_context yield) {
            EXPECT_EQ(request.message(), requestMessage);
            asyncSleep(yield, std::chrono::milliseconds{3});
            return Response(http::status::ok, responseMessage, request);
        });

    EXPECT_CALL(
        *mockWsConnection,
        send(testing::ResultOf([](Response response) { return response.message(); }, responseMessage), testing::_)
    )
        .Times(3)
        .WillRepeatedly(Return(std::nullopt));

    EXPECT_CALL(
        *mockWsConnection,
        send(
            testing::ResultOf(
                [](Response response) { return response.message(); }, "Too many requests for one connection"
            ),
            testing::_
        )
    )
        .Times(2)
        .WillRepeatedly(Return(std::nullopt));

    EXPECT_CALL(onDisconnectMock, Call).WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
        EXPECT_EQ(&c, connectionPtr);
    });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}
