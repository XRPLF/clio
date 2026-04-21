#include "web/ng/impl/ConnectionHandler.hpp"

#include "util/Assert.hpp"
#include "util/CoroutineGroup.hpp"
#include "util/Taggable.hpp"
#include "util/log/Logger.hpp"
#include "web/ProxyIpResolver.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/MessageHandler.hpp"
#include "web/ng/ProcessingPolicy.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/SubscriptionContext.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/websocket/error.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace web::ng::impl {

namespace {

Response
handleHttpRequest(
    ConnectionMetadata& connectionMetadata,
    SubscriptionContextPtr& subscriptionContext,
    ConnectionHandler::TargetToHandlerMap const& handlers,
    Request const& request,
    boost::asio::yield_context yield
)
{
    ASSERT(request.target().has_value(), "Got not a HTTP request");
    auto it = handlers.find(*request.target());
    if (it == handlers.end()) {
        return Response{boost::beast::http::status::bad_request, "Bad target", request};
    }
    return it->second(request, connectionMetadata, subscriptionContext, yield);
}

Response
handleWsRequest(
    ConnectionMetadata& connectionMetadata,
    SubscriptionContextPtr& subscriptionContext,
    std::optional<MessageHandler> const& handler,
    Request const& request,
    boost::asio::yield_context yield
)
{
    if (not handler.has_value()) {
        return Response{
            boost::beast::http::status::bad_request,
            "WebSocket is not supported by this server",
            request
        };
    }
    return handler->operator()(request, connectionMetadata, subscriptionContext, yield);
}

}  // namespace

ConnectionHandler::ConnectionHandler(
    ProcessingPolicy processingPolicy,
    std::optional<size_t> maxParallelRequests,
    util::TagDecoratorFactory& tagFactory,
    std::optional<size_t> maxSubscriptionSendQueueSize,
    ProxyIpResolver proxyIpResolver,
    OnDisconnectHook onDisconnectHook,
    OnIpChangeHook onIpChangeHook
)
    : processingPolicy_{processingPolicy}
    , maxParallelRequests_{maxParallelRequests}
    , tagFactory_{tagFactory}
    , maxSubscriptionSendQueueSize_{maxSubscriptionSendQueueSize}
    , proxyIpResolver_(std::move(proxyIpResolver))
    , onDisconnectHook_{std::move(onDisconnectHook)}
    , onIpChangeHook_(std::move(onIpChangeHook))
{
}

void
ConnectionHandler::onGet(std::string const& target, MessageHandler handler)
{
    getHandlers_[target] = std::move(handler);
}

void
ConnectionHandler::onPost(std::string const& target, MessageHandler handler)
{
    postHandlers_[target] = std::move(handler);
}

void
ConnectionHandler::onWs(MessageHandler handler)
{
    wsHandler_ = std::move(handler);
}

void
ConnectionHandler::processConnection(ConnectionPtr connectionPtr, boost::asio::yield_context yield)
{
    LOG(log_.trace()) << connectionPtr->tag() << "New connection";
    auto& connectionRef = *connectionPtr;

    if (isStopping()) {
        stopConnection(connectionRef, yield);
        return;
    }
    ++connectionsCounter_.get();

    // Using coroutine group here to wait for stopConnection() to finish before exiting this
    // function and destroying connection.
    util::CoroutineGroup stopTask{yield, 1};
    auto stopSignalConnection = onStop_.connect([&connectionRef, &stopTask, yield]() {
        stopTask.spawn(yield, [&connectionRef](boost::asio::yield_context innerYield) {
            stopConnection(connectionRef, innerYield);
        });
    });

    bool shouldCloseGracefully = false;

    std::shared_ptr<SubscriptionContext> subscriptionContext;
    if (connectionRef.wasUpgraded()) {
        auto* ptr = dynamic_cast<impl::WsConnectionBase*>(connectionPtr.get());
        ASSERT(ptr != nullptr, "Casted not websocket connection");
        subscriptionContext = std::make_shared<SubscriptionContext>(
            tagFactory_,
            *ptr,
            maxSubscriptionSendQueueSize_,
            yield,
            [this](Error const& e, Connection const& c) { return handleError(e, c); }
        );
        LOG(log_.trace()) << connectionRef.tag()
                          << "Created SubscriptionContext for the connection";
    }
    SubscriptionContextPtr subscriptionContextInterfacePtr = subscriptionContext;

    switch (processingPolicy_) {
        case ProcessingPolicy::Sequential:
            shouldCloseGracefully =
                sequentRequestResponseLoop(connectionRef, subscriptionContextInterfacePtr, yield);
            break;
        case ProcessingPolicy::Parallel:
            shouldCloseGracefully =
                parallelRequestResponseLoop(connectionRef, subscriptionContextInterfacePtr, yield);
            break;
    }

    if (subscriptionContext != nullptr) {
        subscriptionContext->disconnect(yield);
        LOG(log_.trace()) << connectionRef.tag() << "SubscriptionContext disconnected";
    }

    if (shouldCloseGracefully) {
        connectionRef.setTimeout(kCLOSE_CONNECTION_TIMEOUT);
        connectionRef.close(yield);
        LOG(log_.trace()) << connectionRef.tag() << "Closed gracefully";
    }

    stopSignalConnection.disconnect();
    LOG(log_.trace()) << connectionRef.tag() << "Signal disconnected";

    onDisconnectHook_(connectionRef);
    LOG(log_.trace()) << connectionRef.tag() << "Processing finished";

    // Wait for a stopConnection() to finish if there is any to not have dangling reference in
    // stopConnection().
    stopTask.asyncWait(yield);

    --connectionsCounter_.get();
    if (connectionsCounter_.get().value() == 0 && stopping_)
        stopHelper_.readyToStop();
}

void
ConnectionHandler::stopConnection(Connection& connection, boost::asio::yield_context yield)
{
    util::Logger const log{"WebServer"};
    LOG(log.trace()) << connection.tag() << "Stopping connection";
    Response response{
        boost::beast::http::status::service_unavailable,
        "This Clio node is shutting down. Please try another node.",
        connection
    };
    connection.send(std::move(response), yield);
    connection.setTimeout(kCLOSE_CONNECTION_TIMEOUT);
    connection.close(yield);
    LOG(log.trace()) << connection.tag() << "Connection closed";
}

void
ConnectionHandler::stop(boost::asio::yield_context yield)
{
    *stopping_ = true;
    onStop_();
    if (connectionsCounter_.get().value() == 0)
        return;

    // Wait for server to disconnect all the users
    stopHelper_.asyncWaitForStop(yield);
}

bool
ConnectionHandler::isStopping() const
{
    return *stopping_;
}

bool
ConnectionHandler::handleError(Error const& error, Connection const& connection) const
{
    LOG(log_.trace()) << connection.tag() << "Got error: " << error << " " << error.message();
    // ssl::error::stream_truncated, also known as an SSL "short read",
    // indicates the peer closed the connection without performing the
    // required closing handshake (for example, Google does this to
    // improve performance). Generally this can be a security issue,
    // but if your communication protocol is self-terminated (as
    // it is with both HTTP and WebSocket) then you may simply
    // ignore the lack of close_notify.
    //
    // https://github.com/boostorg/beast/issues/38
    //
    // https://security.stackexchange.com/questions/91435/how-to-handle-a-malicious-ssl-tls-shutdown
    //
    // When a short read would cut off the end of an HTTP message,
    // Beast returns the error boost::beast::http::error::partial_message.
    // Therefore, if we see a short read here, it has occurred
    // after the message has been completed, so it is safe to ignore it.
    if (error == boost::beast::http::error::end_of_stream ||
        error == boost::asio::ssl::error::stream_truncated || error == boost::asio::error::eof ||
        error == boost::beast::error::timeout)
        return false;

    // WebSocket connection was gracefully closed
    if (error == boost::beast::websocket::error::closed)
        return false;

    if (error != boost::asio::error::operation_aborted) {
        LOG(log_.error()) << connection.tag() << ": " << error.message() << ": " << error.value();
    }
    return true;
}

bool
ConnectionHandler::sequentRequestResponseLoop(
    Connection& connection,
    SubscriptionContextPtr& subscriptionContext,
    boost::asio::yield_context yield
)
{
    // The loop here is infinite because:
    // - For websocket connection is persistent so Clio will try to read and respond infinite unless
    // client
    //   disconnected.
    // - When client disconnected connection.send() or connection.receive() will return an error.
    // - For http it is still a loop to reuse the connection if keep alive is set. Otherwise client
    // will disconnect and
    //   an error appears.
    // - When server is shutting down it will cancel all operations on the connection so an error
    // appears.

    LOG(log_.trace()) << connection.tag() << "Processing sequentially";
    while (true) {
        auto const expectedRequest = connection.receive(yield);
        if (not expectedRequest)
            return handleError(expectedRequest.error(), connection);

        resolveClientIp(connection, *expectedRequest);

        auto maybeReturnValue =
            processRequest(connection, subscriptionContext, *expectedRequest, yield);
        if (maybeReturnValue.has_value())
            return maybeReturnValue.value();
    }
}

bool
ConnectionHandler::parallelRequestResponseLoop(
    Connection& connection,
    SubscriptionContextPtr& subscriptionContext,
    boost::asio::yield_context yield
)
{
    LOG(log_.trace()) << connection.tag() << "Processing in parallel";
    // atomic_bool is not needed here because everything happening on coroutine's strand
    bool stop = false;
    bool closeConnectionGracefully = true;
    util::CoroutineGroup tasksGroup{yield, maxParallelRequests_};

    while (not stop) {
        LOG(log_.trace()) << connection.tag() << "Receiving request";
        auto expectedRequest = connection.receive(yield);
        if (not expectedRequest) {
            auto const closeGracefully = handleError(expectedRequest.error(), connection);
            stop = true;
            closeConnectionGracefully &= closeGracefully;
            break;
        }

        resolveClientIp(connection, *expectedRequest);

        if (not tasksGroup.isFull()) {
            bool const spawnSuccess = tasksGroup.spawn(
                yield,  // spawn on the same strand
                [this,
                 &stop,
                 &closeConnectionGracefully,
                 &connection,
                 &subscriptionContext,
                 request = std::move(expectedRequest).value()](
                    boost::asio::yield_context innerYield
                ) mutable {
                    auto maybeCloseConnectionGracefully =
                        processRequest(connection, subscriptionContext, request, innerYield);
                    if (maybeCloseConnectionGracefully.has_value()) {
                        stop = true;
                        closeConnectionGracefully &= maybeCloseConnectionGracefully.value();
                    }
                }
            );
            ASSERT(spawnSuccess, "The coroutine was expected to be spawned");
            LOG(log_.trace()) << connection.tag() << "Spawned a coroutine to process request";
        } else {
            LOG(log_.trace()) << connection.tag()
                              << "Too many requests from one connection, rejecting the request";
            connection.send(
                Response{
                    boost::beast::http::status::too_many_requests,
                    "Too many requests for one connection",
                    expectedRequest.value()
                },
                yield
            );
        }
    }
    LOG(log_.trace()) << connection.tag() << "Waiting processing tasks to finish. Number of tasks: "
                      << tasksGroup.size();
    tasksGroup.asyncWait(yield);
    LOG(log_.trace()) << connection.tag() << "Processing is done";
    return closeConnectionGracefully;
}

std::optional<bool>
ConnectionHandler::processRequest(
    Connection& connection,
    SubscriptionContextPtr& subscriptionContext,
    Request const& request,
    boost::asio::yield_context yield
)
{
    LOG(log_.trace()) << connection.tag() << "Processing request: " << request.message();
    auto response = handleRequest(connection, subscriptionContext, request, yield);

    LOG(log_.trace()) << connection.tag() << "Sending response: " << response.message();
    auto const expectedSuccess = connection.send(std::move(response), yield);
    if (not expectedSuccess.has_value()) {
        return handleError(expectedSuccess.error(), connection);
    }
    return std::nullopt;
}

Response
ConnectionHandler::handleRequest(
    ConnectionMetadata& connectionMetadata,
    SubscriptionContextPtr& subscriptionContext,
    Request const& request,
    boost::asio::yield_context yield
)
{
    switch (request.method()) {
        case Request::Method::Get:
            return handleHttpRequest(
                connectionMetadata, subscriptionContext, getHandlers_, request, yield
            );
        case Request::Method::Post:
            return handleHttpRequest(
                connectionMetadata, subscriptionContext, postHandlers_, request, yield
            );
        case Request::Method::Websocket:
            return handleWsRequest(
                connectionMetadata, subscriptionContext, wsHandler_, request, yield
            );
        default:
            return Response{
                boost::beast::http::status::bad_request, "Unsupported http method", request
            };
    }
}

void
ConnectionHandler::resolveClientIp(Connection& connection, Request const& request) const
{
    auto const updateIp = [&](std::string newIp) {
        if (newIp == connection.ip())
            return;
        LOG(log_.info()) << connection.tag()
                         << "Detected a forwarded request from proxy. Resolved client ip: "
                         << newIp;
        onIpChangeHook_(connection.ip(), newIp);
        connection.setIp(std::move(newIp));
    };

    if (connection.isProxyConnection()) {
        if (auto resolvedIp = ProxyIpResolver::extractClientIp(request.httpHeaders());
            resolvedIp.has_value())
            updateIp(std::move(*resolvedIp));
    } else if (
        auto resolvedIp = proxyIpResolver_.resolveClientIp(connection.ip(), request.httpHeaders());
        resolvedIp.has_value()
    ) {
        updateIp(std::move(*resolvedIp));
        connection.markAsProxyConnection();
    }
}

}  // namespace web::ng::impl
