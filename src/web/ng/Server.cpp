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
#include "util/config/Config.hpp"
#include "util/log/Logger.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/MessageHandler.hpp"
#include "web/ng/impl/HttpConnection.hpp"
#include "web/ng/impl/ServerSslContext.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/beast/core/detect_ssl.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/system/system_error.hpp>
#include <fmt/compile.h>
#include <fmt/core.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace web::ng {

Server::Server(
    util::Config const& config,
    std::unique_ptr<dosguard::DOSGuardInterface> dosguard,
    boost::asio::io_context& ctx
)
    : ctx_{ctx}, dosguard_{std::move(dosguard)}
{
    // TODO(kuznetsss): move this into make_Server()
    auto const serverConfig = config.section("server");

    auto const address = boost::asio::ip::make_address(serverConfig.value<std::string>("ip"));
    auto const port = serverConfig.value<unsigned short>("port");
    endpoint_ = boost::asio::ip::tcp::endpoint{address, port};

    auto expectedSslContext = impl::makeServerSslContext(config);
    if (not expectedSslContext)
        throw std::logic_error{expectedSslContext.error()};
    sslContext_ = std::move(expectedSslContext).value();

    auto adminPassword = serverConfig.maybeValue<std::string>("admin_password");
    auto const localAdmin = serverConfig.maybeValue<bool>("local_admin");

    // Throw config error when localAdmin is true and admin_password is also set
    if (localAdmin && localAdmin.value() && adminPassword) {
        LOG(log_.error()) << "local_admin is true but admin_password is also set, please specify only one method "
                             "to authorize admin";
        throw std::logic_error("Admin config error, local_admin and admin_password can not be set together.");
    }
    // Throw config error when localAdmin is false but admin_password is not set
    if (localAdmin && !localAdmin.value() && !adminPassword) {
        LOG(log_.error()) << "local_admin is false but admin_password is not set, please specify one method "
                             "to authorize admin";
        throw std::logic_error("Admin config error, one method must be specified to authorize admin.");
    }
}

std::optional<std::string>
Server::run()
{
    boost::asio::ip::tcp::acceptor acceptor{ctx_};
    try {
        acceptor.open(endpoint_.protocol());
        acceptor.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor.bind(endpoint_);
        acceptor.listen(boost::asio::socket_base::max_listen_connections);
    } catch (boost::system::system_error const& error) {
        return fmt::format("Web server error: {}", error.what());
    }

    boost::asio::spawn(ctx_, [this, acceptor = std::move(acceptor)](boost::asio::yield_context yield) mutable {
        while (true) {
            boost::beast::error_code errorCode;
            boost::asio::ip::tcp::socket socket{ctx_.get_executor()};

            acceptor.async_accept(socket, yield[errorCode]);
            if (errorCode) {
                LOG(log_.debug()) << "Error accepting a connection: " << errorCode.what();
                continue;
            }
            boost::asio::spawn(ctx_, [this, socket = std::move(socket)](boost::asio::yield_context yield) mutable {
                makeConnection(std::move(socket), yield);
            });  // maybe use boost::asio::detached here?
        }
    });
    return std::nullopt;
}

void
Server::onGet(std::string const& target, MessageHandler handler)
{
    getHandlers_[target] = std::move(handler);
}

void
Server::onPost(std::string const& target, MessageHandler handler)
{
    postHandlers_[target] = std::move(handler);
}

void
Server::onWs(MessageHandler handler)
{
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

    auto connectionTag = connection->tag();

    {
        auto connections = connections_.lock();
        auto [_, inserted] = connections->insert(std::move(connection));
        ASSERT(inserted, "Connection with tag already exists");
    }

    if (upgraded) {
        boost::asio::spawn(
            ctx_,
            [this, connectionTag = std::move(connectionTag)](boost::asio::yield_context yield) mutable {
                handleConnectionLoop(std::move(connectionTag), yield);
            }
        );
    } else {
        boost::asio::spawn(
            ctx_,
            [this, connectionTag = std::move(connectionTag)](boost::asio::yield_context yield) mutable {
                handleConnection(std::move(connectionTag), yield);
            }
        );
    }
}

void
Server::handleConnection(std::string connectionTag, boost::asio::yield_context yield)
{
    // read request from connection
    // process the request
    // send response
}

void
Server::handleConnectionLoop(std::string connectionTag, boost::asio::yield_context yield)
{
    // loop of handleConnection calls
}

}  // namespace web::ng
