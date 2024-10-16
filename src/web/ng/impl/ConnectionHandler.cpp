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

#include "web/ng/impl/ConnectionHandler.hpp"

#include "util/Assert.hpp"
#include "util/log/Logger.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/Error.hpp"
#include "web/ng/MessageHandler.hpp"
#include "web/ng/Request.hpp"
#include "web/ng/Response.hpp"

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/websocket/error.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace web::ng::impl {

namespace {

Response
handleHttpRequest(
    ConnectionContext const& connectionContext,
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
    return it->second(request, connectionContext, yield);
}

Response
handleWsRequest(
    ConnectionContext connectionContext,
    std::optional<MessageHandler> const& handler,
    Request const& request,
    boost::asio::yield_context yield
)
{
    if (not handler.has_value()) {
        return Response{boost::beast::http::status::bad_request, "WebSocket is not supported by this server", request};
    }
    return handler->operator()(request, connectionContext, yield);
}

}  // namespace

size_t
ConnectionHandler::StringHash::operator()(char const* str) const
{
    return hash_type{}(str);
}

size_t
ConnectionHandler::StringHash::operator()(std::string_view str) const
{
    return hash_type{}(str);
}

size_t
ConnectionHandler::StringHash::operator()(std::string const& str) const
{
    return hash_type{}(str);
}

ConnectionHandler::ConnectionHandler(ProcessingPolicy processingPolicy, std::optional<size_t> maxParallelRequests)
    : processingPolicy_{processingPolicy}, maxParallelRequests_{maxParallelRequests}
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
    auto& connectionRef = *connectionPtr;
    auto signalConnection = onStop_.connect([&connectionRef, yield]() { connectionRef.close(yield); });

    bool shouldCloseGracefully = false;

    switch (processingPolicy_) {
        case ProcessingPolicy::Sequential:
            shouldCloseGracefully = sequentRequestResponseLoop(connectionRef, yield);
            break;
        case ProcessingPolicy::Parallel:
            shouldCloseGracefully = parallelRequestResponseLoop(connectionRef, yield);
            break;
    }
    if (shouldCloseGracefully)
        connectionRef.close(yield);

    signalConnection.disconnect();
}

void
ConnectionHandler::stop()
{
    onStop_();
}

bool
ConnectionHandler::handleError(Error const& error, Connection const& connection) const
{
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
    if (error == boost::beast::http::error::end_of_stream || error == boost::asio::ssl::error::stream_truncated)
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
ConnectionHandler::sequentRequestResponseLoop(Connection& connection, boost::asio::yield_context yield)
{
    // The loop here is infinite because:
    // - For websocket connection is persistent so Clio will try to read and respond infinite unless client
    //   disconnected.
    // - When client disconnected connection.send() or connection.receive() will return an error.
    // - For http it is still a loop to reuse the connection if keep alive is set. Otherwise client will disconnect and
    //   an error appears.
    // - When server is shutting down it will cancel all operations on the connection so an error appears.

    while (true) {
        auto expectedRequest = connection.receive(yield);
        if (not expectedRequest)
            return handleError(expectedRequest.error(), connection);

        LOG(log_.info()) << connection.tag() << "Received request from ip = " << connection.ip();

        auto maybeReturnValue = processRequest(connection, std::move(expectedRequest).value(), yield);
        if (maybeReturnValue.has_value())
            return maybeReturnValue.value();
    }
}

bool
ConnectionHandler::parallelRequestResponseLoop(Connection& connection, boost::asio::yield_context yield)
{
    // atomic_bool is not needed here because everything happening on coroutine's strand
    std::optional<bool> closeConnectionGracefully;
    size_t ongoingRequestsCounter{0};

    while (not closeConnectionGracefully.has_value()) {
        auto expectedRequest = connection.receive(yield);
        if (not expectedRequest) {
            return handleError(expectedRequest.error(), connection);
        }

        ++ongoingRequestsCounter;
        if (maxParallelRequests_.has_value() && ongoingRequestsCounter > *maxParallelRequests_) {
            connection.send(
                Response{
                    boost::beast::http::status::too_many_requests,
                    "Too many requests for one session",
                    expectedRequest.value()
                },
                yield
            );
        } else {
            boost::asio::spawn(
                yield,  // spawn on the same strand
                [this,
                 &closeConnectionGracefully,
                 &ongoingRequestsCounter,
                 &connection,
                 request = std::move(expectedRequest).value()](boost::asio::yield_context innerYield) mutable {
                    auto maybeCloseConnectionGracefully = processRequest(connection, request, innerYield);
                    if (maybeCloseConnectionGracefully.has_value()) {
                        if (closeConnectionGracefully.has_value()) {
                            // Close connection gracefully only if both are true. If at least one is false then
                            // connection is already closed.
                            closeConnectionGracefully = *closeConnectionGracefully && *maybeCloseConnectionGracefully;
                        } else {
                            closeConnectionGracefully = maybeCloseConnectionGracefully;
                        }
                    }
                    --ongoingRequestsCounter;
                }
            );
        }
    }
    return *closeConnectionGracefully;
}

std::optional<bool>
ConnectionHandler::processRequest(Connection& connection, Request const& request, boost::asio::yield_context yield)
{
    auto response = handleRequest(connection.context(), request, yield);

    auto const maybeError = connection.send(std::move(response), yield);
    if (maybeError.has_value()) {
        return handleError(maybeError.value(), connection);
    }
    return std::nullopt;
}

Response
ConnectionHandler::handleRequest(
    ConnectionContext const& connectionContext,
    Request const& request,
    boost::asio::yield_context yield
)
{
    switch (request.method()) {
        case Request::Method::GET:
            return handleHttpRequest(connectionContext, getHandlers_, request, yield);
        case Request::Method::POST:
            return handleHttpRequest(connectionContext, postHandlers_, request, yield);
        case Request::Method::WEBSOCKET:
            return handleWsRequest(connectionContext, wsHandler_, request, yield);
        default:
            return Response{boost::beast::http::status::bad_request, "Unsupported http method", request};
    }
}

}  // namespace web::ng::impl
