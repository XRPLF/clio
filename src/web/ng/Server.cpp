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

#include "web/ng/Server.hpp"

#include "util/Assert.hpp"
#include "util/Mutex.hpp"
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"
#include "web/impl/AdminVerificationStrategy.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/MessageHandler.hpp"
#include "web/ng/impl/HttpConnection.hpp"
#include "web/ng/impl/ServerSslContext.hpp"

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/beast/core/detect_ssl.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/system/system_error.hpp>
#include <fmt/compile.h>
#include <fmt/core.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>

namespace web::ng {

namespace {

std::expected<boost::asio::ip::tcp::endpoint, std::string>
makeEndpoint(util::Config const& serverConfig)
{
    auto const ip = serverConfig.maybeValue<std::string>("ip");
    if (not ip.has_value())
        return std::unexpected{"Missing 'ip` in server config."};

    auto const address = boost::asio::ip::make_address(*ip);
    auto const port = serverConfig.maybeValue<unsigned short>("port");
    if (not port.has_value())
        return std::unexpected{"Missing 'port` in server config."};

    return boost::asio::ip::tcp::endpoint{address, *port};
}

std::expected<boost::asio::ip::tcp::acceptor, std::string>
makeAcceptor(boost::asio::io_context& context, boost::asio::ip::tcp::endpoint const& endpoint)
{
    boost::asio::ip::tcp::acceptor acceptor{context};
    try {
        acceptor.open(endpoint.protocol());
        acceptor.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen(boost::asio::socket_base::max_listen_connections);
    } catch (boost::system::system_error const& error) {
        return std::unexpected{fmt::format("Error creating TCP acceptor: {}", error.what())};
    }
    return std::move(acceptor);
}

}  // namespace

Server::Server(
    boost::asio::io_context& ctx,
    boost::asio::ip::tcp::endpoint endpoint,
    std::optional<boost::asio::ssl::context> sslContext,
    std::shared_ptr<web::impl::AdminVerificationStrategy> adminVerificationStrategy,
    std::unique_ptr<dosguard::DOSGuardInterface> dosguard
)
    : ctx_{ctx}
    , dosguard_{std::move(dosguard)}
    , adminVerificationStrategy_(std::move(adminVerificationStrategy))
    , sslContext_{std::move(sslContext)}
    , connections_{std::make_unique<util::Mutex<ConnectionsSet, std::shared_mutex>>()}
    , endpoint_{std::move(endpoint)}
{
}

std::optional<std::string>
Server::run()
{
    auto acceptor = makeAcceptor(ctx_.get(), endpoint_);
    if (not acceptor.has_value())
        return std::move(acceptor).error();

    running_ = true;
    boost::asio::spawn(ctx_, [this, acceptor = std::move(acceptor)](boost::asio::yield_context yield) mutable {
        while (true) {
            boost::beast::error_code errorCode;
            boost::asio::ip::tcp::socket socket{ctx_.get().get_executor()};

            acceptor->async_accept(socket, yield[errorCode]);
            if (errorCode) {
                LOG(log_.debug()) << "Error accepting a connection: " << errorCode.what();
                continue;
            }
            boost::asio::spawn(
                ctx_.get(),
                [this, socket = std::move(socket)](boost::asio::yield_context yield) mutable {
                    makeConnection(std::move(socket), yield);
                },
                boost::asio::detached
            );
        }
    });
    return std::nullopt;
}

void
Server::onGet(std::string const& target, MessageHandler handler)
{
    ASSERT(not running_, "Adding a GET handler is not allowed when Server is running.");
    getHandlers_[target] = std::move(handler);
}

void
Server::onPost(std::string const& target, MessageHandler handler)
{
    ASSERT(not running_, "Adding a POST handler is not allowed when Server is running.");
    postHandlers_[target] = std::move(handler);
}

void
Server::onWs(MessageHandler handler)
{
    ASSERT(not running_, "Adding a Websocket handler is not allowed when Server is running.");
    wsHandler_ = std::move(handler);
}

void
Server::stop()
{
}

void
Server::makeConnection(boost::asio::ip::tcp::socket socket, boost::asio::yield_context yield)
{
    auto const logError = [this](std::string_view message, boost::beast::error_code ec) {
        LOG(log_.info()) << "Detector failed (" << message << "): " << ec.message();
    };

    boost::beast::tcp_stream tcpStream{std::move(socket)};
    boost::beast::flat_buffer buffer;
    boost::beast::error_code errorCode;
    bool const isSsl = boost::beast::async_detect_ssl(tcpStream, buffer, yield[errorCode]);

    if (errorCode == boost::asio::ssl::error::stream_truncated)
        return;

    if (errorCode) {
        logError("detect", errorCode);
        return;
    }

    std::string ip;
    try {
        ip = socket.remote_endpoint().address().to_string();
    } catch (boost::system::system_error const& error) {
        logError("cannot get remote endpoint", error.code());
        return;
    }

    ConnectionPtr connection;
    if (isSsl) {
        if (not sslContext_.has_value()) {
            logError("SSL is not supported by this server", errorCode);
            return;
        }
        connection = std::make_unique<impl::SslHttpConnection>(std::move(socket), *sslContext_);
    } else {
        connection = std::make_unique<impl::HttpConnection>(std::move(socket));
    }

    bool upgraded = false;
    // connection.fetch()
    // if (connection.should_upgrade()) {
    //      connection = std::move(connection).upgrade();
    //      upgraded = true;
    // }

    Connection* connectionPtr = connection.get();

    {
        auto connections = connections_.lock<std::unique_lock>();
        auto [it, inserted] = connections->insert(std::move(connection));
        ASSERT(inserted, "Connection with id {} already exists.", it->get()->id());
    }

    if (upgraded) {
        boost::asio::spawn(ctx_, [this, &connectionRef = *connectionPtr](boost::asio::yield_context yield) mutable {
            handleConnectionLoop(connectionRef, yield);
        });
    } else {
        boost::asio::spawn(ctx_, [this, &connectionRef = *connectionPtr](boost::asio::yield_context yield) mutable {
            handleConnection(connectionRef, yield);
        });
    }
}

void
Server::handleConnection(Connection& connection, boost::asio::yield_context yield)
{
    // auto expectedRequest = connection.receive(yield);
    // if (not expectedRequest.has_value()) {
    // }
    // auto response = handleRequest(std::move(expectedRequest).value());
    // connection.send(std::move(response), yield);
}

void
Server::handleConnectionLoop(Connection& connection, boost::asio::yield_context yield)
{
    // loop of handleConnection calls
}

// Response
// Server::handleRequest(Request request, ConnectionContext connectionContext)
// {
//     auto process = [&connectionContext](Request request, auto& handlersMap) {
//         auto const it = handlersMap.find(request.target());
//         if (it == handlersMap.end()) {
//             return Response{};
//         }
//         return it->second(std::move(request), connectionContext);
//     };
//     switch (request.httpMethod()) {
//         case Request::HttpMethod::GET:
//             return process(std::move(request), getHandlers_);
//         case Request::HttpMethod::POST:
//             return process(std::move(request), postHandlers_);
//         case Request::HttpMethod::WS:
//             if (wsHandler_) {
//                 return (*wsHandler_)(std::move(request));
//             }
//         default:
//             return Response{};
//     }
// }

std::expected<Server, std::string>
make_Server(
    util::Config const& config,
    boost::asio::io_context& context,
    std::unique_ptr<dosguard::DOSGuardInterface> dosguard
)
{
    auto const serverConfig = config.section("server");

    auto endpoint = makeEndpoint(serverConfig);
    if (not endpoint.has_value())
        return std::unexpected{std::move(endpoint).error()};

    auto expectedSslContext = impl::makeServerSslContext(config);
    if (not expectedSslContext)
        return std::unexpected{std::move(expectedSslContext).error()};

    auto adminVerificationStrategy = web::impl::make_AdminVerificationStrategy(serverConfig);
    if (not adminVerificationStrategy) {
        return std::unexpected{std::move(adminVerificationStrategy).error()};
    }

    return Server{
        context,
        std::move(endpoint).value(),
        std::move(expectedSslContext).value(),
        std::move(adminVerificationStrategy).value(),
        std::move(dosguard)

    };
}

}  // namespace web::ng
