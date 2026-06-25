#include "util/AsioContextTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/Spawn.hpp"
#include "util/Taggable.hpp"
#include "util/UnsupportedType.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "web/ProxyIpResolver.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/ProcessingPolicy.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/ConnectionHandler.hpp"
#include "web/ng/impl/MockHttpConnection.hpp"
#include "web/ng/impl/MockWsConnection.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/websocket/error.hpp>
#include <fmt/format.h>
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
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;

struct ConnectionHandlerTest : prometheus::WithPrometheus, SyncAsioContextTest {
    ConnectionHandlerTest(ProcessingPolicy policy, std::optional<size_t> maxParallelConnections)
        : tagFactory{util::config::ClioConfigDefinition{
              {"log.tag_style",
               config::ConfigValue{config::ConfigType::String}.defaultValue("uint")}
          }}
        , connectionHandler{
              policy,
              maxParallelConnections,
              tagFactory,
              std::nullopt,
              proxyIpResolver,
              onDisconnectMock.AsStdFunction(),
              onIpChangeMock.AsStdFunction()
          }
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
        } else if constexpr (
            std::same_as<BoostErrorType, boost::asio::error::basic_errors> ||
            std::same_as<BoostErrorType, boost::asio::error::misc_errors> ||
            std::same_as<BoostErrorType, boost::asio::error::addrinfo_errors> ||
            std::same_as<BoostErrorType, boost::asio::error::netdb_errors>
        ) {
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

    std::string const clientIp = "1.2.3.4";
    std::string const proxyIp = "5.6.7.8";
    std::string const proxyToken = "some_proxy_token";
    web::ProxyIpResolver proxyIpResolver{{proxyIp}, {proxyToken}};

    testing::StrictMock<testing::MockFunction<void(std::string const&, std::string const&)>>
        onIpChangeMock;
    testing::StrictMock<testing::MockFunction<void(Connection const&)>> onDisconnectMock;
    util::TagDecoratorFactory tagFactory;
    ConnectionHandler connectionHandler;

    util::TagDecoratorFactory tagDecoratorFactory{config::ClioConfigDefinition{
        {"log.tag_style", config::ConfigValue{config::ConfigType::String}.defaultValue("uint")}
    }};
    StrictMockHttpConnectionPtr mockHttpConnection = std::make_unique<StrictMockHttpConnection>(
        clientIp,
        boost::beast::flat_buffer{},
        tagDecoratorFactory
    );
    StrictMockWsConnectionPtr mockWsConnection = std::make_unique<StrictMockWsConnection>(
        clientIp,
        boost::beast::flat_buffer{},
        tagDecoratorFactory
    );

    Request::HttpHeaders headers;
};

struct ConnectionHandlerSequentialProcessingTest : ConnectionHandlerTest {
    ConnectionHandlerSequentialProcessingTest()
        : ConnectionHandlerTest(ProcessingPolicy::Sequential, std::nullopt)
    {
    }
};

TEST_F(ConnectionHandlerSequentialProcessingTest, ReceiveError)
{
    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(Return(makeError(http::error::end_of_stream)));
    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, ReceiveError_CloseConnection)
{
    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(Return(makeError(boost::asio::error::timed_out)));
    EXPECT_CALL(
        *mockHttpConnection,
        setTimeout(std::chrono::steady_clock::duration{ConnectionHandler::kCloseConnectionTimeout})
    );
    EXPECT_CALL(*mockHttpConnection, close);
    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
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

    EXPECT_CALL(*mockHttpConnection, send)
        .WillOnce([](Response response, auto&&) -> std::expected<void, web::ng::Error> {
            EXPECT_EQ(response.message(), "WebSocket is not supported by this server");
            return {};
        });

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
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
        .WillOnce(Return(makeRequest(
            http::request<http::string_body>{http::verb::get, target, 11, requestMessage}
        )))
        .WillOnce(Return(makeError(http::error::end_of_stream)));

    EXPECT_CALL(*mockHttpConnection, send)
        .WillOnce([](Response response, auto&&) -> std::expected<void, web::ng::Error> {
            EXPECT_EQ(response.message(), "Bad target");
            auto const httpResponse = std::move(response).intoHttpResponse();
            EXPECT_EQ(httpResponse.result(), http::status::bad_request);
            EXPECT_EQ(httpResponse.version(), 11);
            return {};
        });

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
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

    EXPECT_CALL(*mockHttpConnection, send)
        .WillOnce([](Response response, auto&&) -> std::expected<void, web::ng::Error> {
            EXPECT_EQ(response.message(), "Unsupported http method");
            return {};
        });

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_Send)
{
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
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

    EXPECT_CALL(*mockWsConnection, send)
        .WillOnce(
            [&responseMessage](Response response, auto&&) -> std::expected<void, web::ng::Error> {
                EXPECT_EQ(response.message(), responseMessage);
                return {};
            }
        );

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, SendSubscriptionMessage)
{
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    std::string const subscriptionMessage = "subscription message";

    EXPECT_CALL(*mockWsConnection, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(*mockWsConnection, receive)
        .WillOnce(Return(makeRequest("", headers)))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call)
        .WillOnce([&](Request const& request,
                      auto&&,
                      web::SubscriptionContextPtr subscriptionContext,
                      auto&&) {
            EXPECT_NE(subscriptionContext, nullptr);
            subscriptionContext->send(std::make_shared<std::string>(subscriptionMessage));
            return Response(http::status::ok, "", request);
        });

    EXPECT_CALL(*mockWsConnection, send).WillOnce(Return(std::expected<void, web::ng::Error>{}));

    EXPECT_CALL(*mockWsConnection, sendShared)
        .WillOnce(
            [&subscriptionMessage](
                std::shared_ptr<std::string> sendingMessage, auto&&
            ) -> std::expected<void, web::ng::Error> {
                EXPECT_EQ(*sendingMessage, subscriptionMessage);
                return {};
            }
        );

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(
    ConnectionHandlerSequentialProcessingTest,
    SubscriptionContextIsDisconnectedAfterProcessingFinished
)
{
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    testing::StrictMock<testing::MockFunction<void(web::SubscriptionContextInterface*)>>
        onDisconnectHook;

    EXPECT_CALL(*mockWsConnection, wasUpgraded).WillOnce(Return(true));
    testing::Expectation const expectationReceiveCalled =
        EXPECT_CALL(*mockWsConnection, receive)
            .WillOnce(Return(makeRequest("", headers)))
            .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(wsHandlerMock, Call)
        .WillOnce([&](Request const& request,
                      auto&&,
                      web::SubscriptionContextPtr subscriptionContext,
                      auto&&) {
            EXPECT_NE(subscriptionContext, nullptr);
            subscriptionContext->onDisconnect(onDisconnectHook.AsStdFunction());
            return Response(http::status::ok, "", request);
        });

    EXPECT_CALL(*mockWsConnection, send).WillOnce(Return(std::expected<void, web::ng::Error>{}));

    EXPECT_CALL(onDisconnectHook, Call).After(expectationReceiveCalled);

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, SubscriptionContextIsNullForHttpConnection)
{
    std::string const target = "/some/target";
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        postHandlerMock;
    connectionHandler.onPost(target, postHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    auto const returnRequest = Return(
        makeRequest(http::request<http::string_body>{http::verb::post, target, 11, requestMessage})
    );

    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(returnRequest)
        .WillOnce(Return(makeError(http::error::partial_message)));

    EXPECT_CALL(postHandlerMock, Call)
        .WillOnce([&](Request const& request,
                      auto&&,
                      web::SubscriptionContextPtr subscriptionContext,
                      auto&&) {
            EXPECT_EQ(subscriptionContext, nullptr);

            return Response(http::status::ok, responseMessage, request);
        });

    EXPECT_CALL(*mockHttpConnection, send)
        .WillOnce(
            [&responseMessage](Response response, auto&&) -> std::expected<void, web::ng::Error> {
                EXPECT_EQ(response.message(), responseMessage);
                return {};
            }
        );

    EXPECT_CALL(
        *mockHttpConnection,
        setTimeout(std::chrono::steady_clock::duration{ConnectionHandler::kCloseConnectionTimeout})
    );
    EXPECT_CALL(*mockHttpConnection, close);

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_Send_Loop)
{
    std::string const target = "/some/target";
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        postHandlerMock;
    connectionHandler.onPost(target, postHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    auto const returnRequest = Return(
        makeRequest(http::request<http::string_body>{http::verb::post, target, 11, requestMessage})
    );

    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(returnRequest)
        .WillOnce(Return(makeError(http::error::partial_message)));

    EXPECT_CALL(postHandlerMock, Call)
        .Times(3)
        .WillRepeatedly([&](Request const& request, auto&&, auto&&, auto&&) {
            EXPECT_EQ(request.message(), requestMessage);
            return Response(http::status::ok, responseMessage, request);
        });

    EXPECT_CALL(*mockHttpConnection, send)
        .Times(3)
        .WillRepeatedly(
            [&responseMessage](Response response, auto&&) -> std::expected<void, web::ng::Error> {
                EXPECT_EQ(response.message(), responseMessage);
                return {};
            }
        );

    EXPECT_CALL(
        *mockHttpConnection,
        setTimeout(std::chrono::steady_clock::duration{ConnectionHandler::kCloseConnectionTimeout})
    );
    EXPECT_CALL(*mockHttpConnection, close);

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Receive_Handle_SendError)
{
    std::string const target = "/some/target";
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        getHandlerMock;

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    connectionHandler.onGet(target, getHandlerMock.AsStdFunction());

    EXPECT_CALL(*mockHttpConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(Return(makeRequest(
            http::request<http::string_body>{http::verb::get, target, 11, requestMessage}
        )));

    EXPECT_CALL(getHandlerMock, Call).WillOnce([&](Request const& request, auto&&, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockHttpConnection, send).WillOnce([&responseMessage](Response response, auto&&) {
        EXPECT_EQ(response.message(), responseMessage);
        return makeError(http::error::end_of_stream);
    });

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, OnIpChangeHookCalledWhenSentFromProxy)
{
    std::string const target = "/some/target";
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        getHandlerMock;

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    connectionHandler.onGet(target, getHandlerMock.AsStdFunction());

    StrictMockHttpConnectionPtr mockHttpConnectionFromProxy =
        std::make_unique<StrictMockHttpConnection>(
            proxyIp, boost::beast::flat_buffer{}, tagDecoratorFactory
        );

    auto request = http::request<http::string_body>{http::verb::get, target, 11, requestMessage};
    request.set(http::field::forwarded, fmt::format("for={}", clientIp));

    EXPECT_CALL(*mockHttpConnectionFromProxy, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockHttpConnectionFromProxy, receive).WillOnce(Return(makeRequest(request)));

    EXPECT_CALL(onIpChangeMock, Call(proxyIp, clientIp));

    EXPECT_CALL(getHandlerMock, Call).WillOnce([&](Request const& request, auto&&, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockHttpConnectionFromProxy, send)
        .WillOnce([&responseMessage](Response response, auto&&) {
            EXPECT_EQ(response.message(), responseMessage);
            return makeError(http::error::end_of_stream);
        });

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([this, connectionPtr = mockHttpConnectionFromProxy.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
            EXPECT_EQ(c.ip(), clientIp);
        });

    runSpawn([this, c = std::move(mockHttpConnectionFromProxy)](
                 boost::asio::yield_context yield
             ) mutable { connectionHandler.processConnection(std::move(c), yield); });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, ProxyConnection_SameClientReuses_HookCalledOnce)
{
    std::string const target = "/some/target";
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        getHandlerMock;
    connectionHandler.onGet(target, getHandlerMock.AsStdFunction());

    StrictMockHttpConnectionPtr mockProxyConnection = std::make_unique<StrictMockHttpConnection>(
        proxyIp, boost::beast::flat_buffer{}, tagDecoratorFactory
    );

    auto request = http::request<http::string_body>{http::verb::get, target, 11, ""};
    request.set(http::field::forwarded, fmt::format("for={}", clientIp));

    EXPECT_CALL(*mockProxyConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockProxyConnection, receive)
        .WillOnce(Return(makeRequest(request)))
        .WillOnce(Return(makeRequest(request)))
        .WillOnce(Return(makeError(http::error::end_of_stream)));

    EXPECT_CALL(onIpChangeMock, Call(proxyIp, clientIp));

    EXPECT_CALL(getHandlerMock, Call)
        .Times(2)
        .WillRepeatedly([](Request const& req, auto&&, auto&&, auto&&) {
            return Response(http::status::ok, "ok", req);
        });

    EXPECT_CALL(*mockProxyConnection, send)
        .Times(2)
        .WillRepeatedly(Return(std::expected<void, web::ng::Error>{}));

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([this, ptr = mockProxyConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, ptr);
            EXPECT_EQ(c.ip(), clientIp);
        });

    runSpawn([this, c = std::move(mockProxyConnection)](boost::asio::yield_context yield) mutable {
        connectionHandler.processConnection(std::move(c), yield);
    });
}

TEST_F(
    ConnectionHandlerSequentialProcessingTest,
    ProxyConnection_DifferentClientReuses_HookCalledForEachIpChange
)
{
    std::string const target = "/some/target";
    std::string const anotherClientIp = "9.10.11.12";
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        getHandlerMock;
    connectionHandler.onGet(target, getHandlerMock.AsStdFunction());

    StrictMockHttpConnectionPtr mockProxyConnection = std::make_unique<StrictMockHttpConnection>(
        proxyIp, boost::beast::flat_buffer{}, tagDecoratorFactory
    );

    auto request1 = http::request<http::string_body>{http::verb::get, target, 11, ""};
    request1.set(http::field::forwarded, fmt::format("for={}", clientIp));

    auto request2 = http::request<http::string_body>{http::verb::get, target, 11, ""};
    request2.set(http::field::forwarded, fmt::format("for={}", anotherClientIp));

    EXPECT_CALL(*mockProxyConnection, wasUpgraded).WillOnce(Return(false));
    EXPECT_CALL(*mockProxyConnection, receive)
        .WillOnce(Return(makeRequest(request1)))
        .WillOnce(Return(makeRequest(request2)))
        .WillOnce(Return(makeError(http::error::end_of_stream)));

    EXPECT_CALL(onIpChangeMock, Call(proxyIp, clientIp));
    EXPECT_CALL(onIpChangeMock, Call(clientIp, anotherClientIp));

    EXPECT_CALL(getHandlerMock, Call)
        .Times(2)
        .WillRepeatedly([](Request const& req, auto&&, auto&&, auto&&) {
            return Response(http::status::ok, "ok", req);
        });

    EXPECT_CALL(*mockProxyConnection, send)
        .Times(2)
        .WillRepeatedly(Return(std::expected<void, web::ng::Error>{}));

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([anotherClientIp, ptr = mockProxyConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, ptr);
            EXPECT_EQ(c.ip(), anotherClientIp);
        });

    runSpawn([this, c = std::move(mockProxyConnection)](boost::asio::yield_context yield) mutable {
        connectionHandler.processConnection(std::move(c), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, Stop)
{
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";
    bool connectionClosed = false;

    EXPECT_CALL(*mockWsConnection, wasUpgraded).Times(2).WillRepeatedly(Return(true));
    EXPECT_CALL(*mockWsConnection, receive)
        .Times(4)
        .WillRepeatedly([&](auto&&) -> std::expected<Request, Error> {
            if (connectionClosed) {
                return makeError(websocket::error::closed);
            }
            return makeRequest(requestMessage, headers);
        });

    EXPECT_CALL(wsHandlerMock, Call)
        .Times(3)
        .WillRepeatedly([&](Request const& request, auto&&, auto&&, auto&&) {
            EXPECT_EQ(request.message(), requestMessage);
            return Response(http::status::ok, responseMessage, request);
        });

    size_t numCalls = 0;
    EXPECT_CALL(
        *mockWsConnection,
        send(
            testing::ResultOf([](Response const& r) { return r.message(); }, responseMessage),
            testing::_
        )
    )
        .Times(3)
        .WillRepeatedly([&](auto&&, auto&&) -> std::expected<void, web::ng::Error> {
            ++numCalls;
            if (numCalls == 3)
                util::spawn(ctx_, [this](auto yield) { connectionHandler.stop(yield); });

            return {};
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
        *mockWsConnection,
        setTimeout(std::chrono::steady_clock::duration{ConnectionHandler::kCloseConnectionTimeout})
    );
    EXPECT_CALL(*mockWsConnection, close).WillOnce([&connectionClosed]() {
        connectionClosed = true;
    });

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerSequentialProcessingTest, ProcessCalledAfterStop)
{
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    runSyncOperation([this](boost::asio::yield_context yield) { connectionHandler.stop(yield); });

    EXPECT_CALL(*mockWsConnection, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(
        *mockWsConnection,
        send(
            testing::ResultOf(
                [](Response const& r) { return r.message(); },
                testing::HasSubstr("This Clio node is shutting down")
            ),
            testing::_
        )
    );

    EXPECT_CALL(
        *mockWsConnection,
        setTimeout(std::chrono::steady_clock::duration{ConnectionHandler::kCloseConnectionTimeout})
    );
    EXPECT_CALL(*mockWsConnection, close);

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

struct ConnectionHandlerParallelProcessingTest : ConnectionHandlerTest {
    static constexpr size_t kMaxParallelRequests = 3;

    ConnectionHandlerParallelProcessingTest()
        : ConnectionHandlerTest(
              ProcessingPolicy::Parallel,
              ConnectionHandlerParallelProcessingTest::kMaxParallelRequests
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
    EXPECT_CALL(*mockHttpConnection, receive)
        .WillOnce(Return(makeError(http::error::end_of_stream)));

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockHttpConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockHttpConnection), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, Receive_Handle_Send)
{
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
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

    EXPECT_CALL(*mockWsConnection, send)
        .WillOnce(
            [&responseMessage](Response response, auto&&) -> std::expected<void, web::ng::Error> {
                EXPECT_EQ(response.message(), responseMessage);
                return {};
            }
        );

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, OnIpChangeHookCalledWhenSentFromProxy)
{
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
        wsHandlerMock;
    connectionHandler.onWs(wsHandlerMock.AsStdFunction());

    StrictMockWsConnectionPtr mockWsConnectionFromProxy = std::make_unique<StrictMockWsConnection>(
        proxyIp, boost::beast::flat_buffer{}, tagDecoratorFactory
    );
    headers.set(http::field::forwarded, fmt::format("for={}", clientIp));

    std::string const requestMessage = "some message";
    std::string const responseMessage = "some response";

    EXPECT_CALL(*mockWsConnectionFromProxy, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(*mockWsConnectionFromProxy, receive)
        .WillOnce(Return(makeRequest(requestMessage, headers)))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(onIpChangeMock, Call(proxyIp, clientIp));

    EXPECT_CALL(wsHandlerMock, Call).WillOnce([&](Request const& request, auto&&, auto&&, auto&&) {
        EXPECT_EQ(request.message(), requestMessage);
        return Response(http::status::ok, responseMessage, request);
    });

    EXPECT_CALL(*mockWsConnectionFromProxy, send)
        .WillOnce(
            [&responseMessage](Response response, auto&&) -> std::expected<void, web::ng::Error> {
                EXPECT_EQ(response.message(), responseMessage);
                return {};
            }
        );

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([this, connectionPtr = mockWsConnectionFromProxy.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
            EXPECT_EQ(c.ip(), clientIp);
        });

    runSpawn([this,
              c = std::move(mockWsConnectionFromProxy)](boost::asio::yield_context yield) mutable {
        connectionHandler.processConnection(std::move(c), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, ProxyConnection_SameClientReuses_HookCalledOnce)
{
    connectionHandler.onWs([](Request const& req, auto&&, auto&&, auto&&) {
        return Response(http::status::ok, "ok", req);
    });

    StrictMockWsConnectionPtr mockProxyConnection = std::make_unique<StrictMockWsConnection>(
        proxyIp, boost::beast::flat_buffer{}, tagDecoratorFactory
    );

    headers.set(http::field::forwarded, fmt::format("for={}", clientIp));

    EXPECT_CALL(*mockProxyConnection, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(*mockProxyConnection, receive)
        .WillOnce(Return(makeRequest("msg", headers)))
        .WillOnce(Return(makeRequest("msg", headers)))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(onIpChangeMock, Call(proxyIp, clientIp));

    EXPECT_CALL(*mockProxyConnection, send)
        .Times(2)
        .WillRepeatedly(Return(std::expected<void, web::ng::Error>{}));

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([this, ptr = mockProxyConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, ptr);
            EXPECT_EQ(c.ip(), clientIp);
        });

    runSpawn([this, c = std::move(mockProxyConnection)](boost::asio::yield_context yield) mutable {
        connectionHandler.processConnection(std::move(c), yield);
    });
}

TEST_F(
    ConnectionHandlerParallelProcessingTest,
    ProxyConnection_DifferentClientReuses_HookCalledForEachIpChange
)
{
    std::string const anotherClientIp = "9.10.11.12";
    connectionHandler.onWs([](Request const& req, auto&&, auto&&, auto&&) {
        return Response(http::status::ok, "ok", req);
    });

    StrictMockWsConnectionPtr mockProxyConnection = std::make_unique<StrictMockWsConnection>(
        proxyIp, boost::beast::flat_buffer{}, tagDecoratorFactory
    );

    Request::HttpHeaders headers1;
    headers1.set(http::field::forwarded, fmt::format("for={}", clientIp));

    Request::HttpHeaders headers2;
    headers2.set(http::field::forwarded, fmt::format("for={}", anotherClientIp));

    EXPECT_CALL(*mockProxyConnection, wasUpgraded).WillOnce(Return(true));
    EXPECT_CALL(*mockProxyConnection, receive)
        .WillOnce(Return(makeRequest("msg", headers1)))
        .WillOnce(Return(makeRequest("msg", headers2)))
        .WillOnce(Return(makeError(websocket::error::closed)));

    EXPECT_CALL(onIpChangeMock, Call(proxyIp, clientIp));
    EXPECT_CALL(onIpChangeMock, Call(clientIp, anotherClientIp));

    EXPECT_CALL(*mockProxyConnection, send)
        .Times(2)
        .WillRepeatedly(Return(std::expected<void, web::ng::Error>{}));

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([anotherClientIp, ptr = mockProxyConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, ptr);
            EXPECT_EQ(c.ip(), anotherClientIp);
        });

    runSpawn([this, c = std::move(mockProxyConnection)](boost::asio::yield_context yield) mutable {
        connectionHandler.processConnection(std::move(c), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, Receive_Handle_Send_Loop)
{
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
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

    EXPECT_CALL(wsHandlerMock, Call)
        .Times(2)
        .WillRepeatedly([&](Request const& request, auto&&, auto&&, auto&&) {
            EXPECT_EQ(request.message(), requestMessage);
            return Response(http::status::ok, responseMessage, request);
        });

    EXPECT_CALL(*mockWsConnection, send)
        .Times(2)
        .WillRepeatedly(
            [&responseMessage](Response response, auto&&) -> std::expected<void, web::ng::Error> {
                EXPECT_EQ(response.message(), responseMessage);
                return {};
            }
        );

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}

TEST_F(ConnectionHandlerParallelProcessingTest, Receive_Handle_Send_Loop_TooManyRequest)
{
    testing::StrictMock<testing::MockFunction<Response(
        Request const&,
        ConnectionMetadata const&,
        web::SubscriptionContextPtr,
        boost::asio::yield_context
    )>>
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
        .WillRepeatedly(
            [&](Request const& request, auto&&, auto&&, boost::asio::yield_context yield) {
                EXPECT_EQ(request.message(), requestMessage);
                asyncSleep(yield, std::chrono::milliseconds{3});
                return Response(http::status::ok, responseMessage, request);
            }
        );

    EXPECT_CALL(
        *mockWsConnection,
        send(
            testing::ResultOf(
                [](Response response) { return response.message(); }, responseMessage
            ),
            testing::_
        )
    )
        .Times(3)
        .WillRepeatedly(Return(std::expected<void, web::ng::Error>{}));

    EXPECT_CALL(
        *mockWsConnection,
        send(
            testing::ResultOf(
                [](Response response) { return response.message(); },
                "Too many requests for one connection"
            ),
            testing::_
        )
    )
        .Times(2)
        .WillRepeatedly(Return(std::expected<void, web::ng::Error>{}));

    EXPECT_CALL(onDisconnectMock, Call)
        .WillOnce([connectionPtr = mockWsConnection.get()](Connection const& c) {
            EXPECT_EQ(&c, connectionPtr);
        });

    runSpawn([this](boost::asio::yield_context yield) {
        connectionHandler.processConnection(std::move(mockWsConnection), yield);
    });
}
