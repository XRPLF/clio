//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#pragma once

#include "util/Taggable.hpp"
#include "util/log/Logger.hpp"
#include "web/DOSGuard.hpp"
#include "web/HttpSession.hpp"
#include "web/SslHttpSession.hpp"
#include "web/interface/Concepts.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <fmt/core.h>

#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

/**
 * @brief This namespace implements the web server and related components.
 *
 * The web server is leveraging the power of `boost::asio` with it's coroutine support thru `boost::asio::yield_context`
 * and `boost::asio::spawn`.
 *
 * Majority of the code is based on examples that came with boost.
 */
namespace web {

/**
 * @brief The Detector class to detect if the connection is a ssl or not.
 *
 * If it is an SSL connection, the Detector will pass the ownership of the socket to SslSessionType, otherwise to
 * PlainSessionType.
 *
 * @tparam PlainSessionType The plain session type
 * @tparam SslSessionType The SSL session type
 * @tparam HandlerType The executor to handle the requests
 */
template <template <class> class PlainSessionType, template <class> class SslSessionType, SomeServerHandler HandlerType>
class Detector : public std::enable_shared_from_this<Detector<PlainSessionType, SslSessionType, HandlerType>> {
    using std::enable_shared_from_this<Detector<PlainSessionType, SslSessionType, HandlerType>>::shared_from_this;

    util::Logger log_{"WebServer"};
    boost::beast::tcp_stream stream_;
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> ctx_;
    std::reference_wrapper<util::TagDecoratorFactory const> tagFactory_;
    std::reference_wrapper<web::DOSGuard> const dosGuard_;
    std::shared_ptr<HandlerType> const handler_;
    boost::beast::flat_buffer buffer_;
    std::shared_ptr<impl::AdminVerificationStrategy> const adminVerification_;

public:
    /**
     * @brief Create a new detector.
     *
     * @param socket The socket. Ownership is transferred
     * @param ctx The SSL context if any
     * @param tagFactory A factory that is used to generate tags to track requests and sessions
     * @param dosGuard The denial of service guard to use
     * @param handler The server handler to use
     * @param adminPassword The optional password to verify admin role in requests
     */
    Detector(
        tcp::socket&& socket,
        std::optional<std::reference_wrapper<boost::asio::ssl::context>> ctx,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<web::DOSGuard> dosGuard,
        std::shared_ptr<HandlerType> handler,
        std::shared_ptr<impl::AdminVerificationStrategy> adminVerification
    )
        : stream_(std::move(socket))
        , ctx_(ctx)
        , tagFactory_(std::cref(tagFactory))
        , dosGuard_(dosGuard)
        , handler_(std::move(handler))
        , adminVerification_(std::move(adminVerification))
    {
    }

    /**
     * @brief A helper function that is called when any error ocurs.
     *
     * @param ec The error code
     * @param message The message to include in the log
     */
    inline void
    fail(boost::system::error_code ec, char const* message)
    {
        if (ec == boost::asio::ssl::error::stream_truncated)
            return;

        LOG(log_.info()) << "Detector failed (" << message << "): " << ec.message();
    }

    /** @brief Initiate the detection. */
    void
    run()
    {
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
        async_detect_ssl(stream_, buffer_, boost::beast::bind_front_handler(&Detector::onDetect, shared_from_this()));
    }

    /**
     * @brief Handles detection result.
     *
     * @param ec The error code
     * @param result true if SSL is detected; false otherwise
     */
    void
    onDetect(boost::beast::error_code ec, bool result)
    {
        if (ec)
            return fail(ec, "detect");

        std::string ip;
        try {
            ip = stream_.socket().remote_endpoint().address().to_string();
        } catch (std::exception const&) {
            return fail(ec, "cannot get remote endpoint");
        }

        if (result) {
            if (!ctx_)
                return fail(ec, "SSL is not supported by this server");

            std::make_shared<SslSessionType<HandlerType>>(
                stream_.release_socket(),
                ip,
                adminVerification_,
                *ctx_,
                tagFactory_,
                dosGuard_,
                handler_,
                std::move(buffer_)
            )
                ->run();
            return;
        }

        std::make_shared<PlainSessionType<HandlerType>>(
            stream_.release_socket(), ip, adminVerification_, tagFactory_, dosGuard_, handler_, std::move(buffer_)
        )
            ->run();
    }
};

/**
 * @brief The WebServer class. It creates server socket and start listening on it.
 *
 * Once there is client connection, it will accept it and pass the socket to Detector to detect ssl or plain.
 *
 * @tparam PlainSessionType The plain session to handle non-ssl connection.
 * @tparam SslSessionType The SSL session to handle SSL connection.
 * @tparam HandlerType The handler to process the request and return response.
 */
template <template <class> class PlainSessionType, template <class> class SslSessionType, SomeServerHandler HandlerType>
class Server : public std::enable_shared_from_this<Server<PlainSessionType, SslSessionType, HandlerType>> {
    using std::enable_shared_from_this<Server<PlainSessionType, SslSessionType, HandlerType>>::shared_from_this;

    util::Logger log_{"WebServer"};
    std::reference_wrapper<boost::asio::io_context> ioc_;
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> ctx_;
    util::TagDecoratorFactory tagFactory_;
    std::reference_wrapper<web::DOSGuard> dosGuard_;
    std::shared_ptr<HandlerType> handler_;
    tcp::acceptor acceptor_;
    std::shared_ptr<impl::AdminVerificationStrategy> adminVerification_;

public:
    /**
     * @brief Create a new instance of the web server.
     *
     * @param ioc The io_context to run the server on
     * @param ctx The SSL context if any
     * @param endpoint The endpoint to listen on
     * @param tagFactory A factory that is used to generate tags to track requests and sessions
     * @param dosGuard The denial of service guard to use
     * @param handler The server handler to use
     * @param adminPassword The optional password to verify admin role in requests
     */
    Server(
        boost::asio::io_context& ioc,
        std::optional<std::reference_wrapper<boost::asio::ssl::context>> ctx,
        tcp::endpoint endpoint,
        util::TagDecoratorFactory tagFactory,
        web::DOSGuard& dosGuard,
        std::shared_ptr<HandlerType> handler,
        std::optional<std::string> adminPassword
    )
        : ioc_(std::ref(ioc))
        , ctx_(ctx)
        , tagFactory_(tagFactory)
        , dosGuard_(std::ref(dosGuard))
        , handler_(std::move(handler))
        , acceptor_(boost::asio::make_strand(ioc))
        , adminVerification_(impl::make_AdminVerificationStrategy(std::move(adminPassword)))
    {
        boost::beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
            return;

        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec)
            return;

        acceptor_.bind(endpoint, ec);
        if (ec) {
            LOG(log_.error()) << "Failed to bind to endpoint: " << endpoint << ". message: " << ec.message();
            throw std::runtime_error(
                fmt::format("Failed to bind to endpoint: {}:{}", endpoint.address().to_string(), endpoint.port())
            );
        }

        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec) {
            LOG(log_.error()) << "Failed to listen at endpoint: " << endpoint << ". message: " << ec.message();
            throw std::runtime_error(
                fmt::format("Failed to listen at endpoint: {}:{}", endpoint.address().to_string(), endpoint.port())
            );
        }
    }

    /** @brief Start accepting incoming connections. */
    void
    run()
    {
        doAccept();
    }

private:
    void
    doAccept()
    {
        acceptor_.async_accept(
            boost::asio::make_strand(ioc_.get()),
            boost::beast::bind_front_handler(&Server::onAccept, shared_from_this())
        );
    }

    void
    onAccept(boost::beast::error_code ec, tcp::socket socket)
    {
        if (!ec) {
            auto ctxRef =
                ctx_ ? std::optional<std::reference_wrapper<boost::asio::ssl::context>>{ctx_.value()} : std::nullopt;

            std::make_shared<Detector<PlainSessionType, SslSessionType, HandlerType>>(
                std::move(socket), ctxRef, std::cref(tagFactory_), dosGuard_, handler_, adminVerification_
            )
                ->run();
        }

        doAccept();
    }
};

/** @brief The final type of the HttpServer used by Clio. */
template <class HandlerType>
using HttpServer = Server<HttpSession, SslHttpSession, HandlerType>;

/**
 * @brief A factory function that spawns a ready to use HTTP server.
 *
 * @tparam HandlerType The tyep of handler to process the request
 * @param config The config to create server
 * @param ioc The server will run under this io_context
 * @param ctx The SSL context if any
 * @param dosGuard The dos guard to protect the server
 * @param handler The handler to process the request
 */
template <class HandlerType>
static std::shared_ptr<HttpServer<HandlerType>>
make_HttpServer(
    util::Config const& config,
    boost::asio::io_context& ioc,
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> const& ctx,
    web::DOSGuard& dosGuard,
    std::shared_ptr<HandlerType> const& handler
)
{
    static util::Logger const log{"WebServer"};
    if (!config.contains("server"))
        return nullptr;

    auto const serverConfig = config.section("server");
    auto const address = boost::asio::ip::make_address(serverConfig.value<std::string>("ip"));
    auto const port = serverConfig.value<unsigned short>("port");
    auto adminPassword = serverConfig.maybeValue<std::string>("admin_password");
    auto const localAdmin = serverConfig.maybeValue<bool>("local_admin");

    // Throw config error when localAdmin is true and admin_password is also set
    if (localAdmin && localAdmin.value() && adminPassword) {
        LOG(log.error()) << "local_admin is true but admin_password is also set, please specify only one method "
                            "to authorize admin";
        throw std::logic_error("Admin config error, local_admin and admin_password can not be set together.");
    }
    // Throw config error when localAdmin is false but admin_password is not set
    if (localAdmin && !localAdmin.value() && !adminPassword) {
        LOG(log.error()) << "local_admin is false but admin_password is not set, please specify one method "
                            "to authorize admin";
        throw std::logic_error("Admin config error, one method must be specified to authorize admin.");
    }

    auto server = std::make_shared<HttpServer<HandlerType>>(
        ioc,
        ctx,
        boost::asio::ip::tcp::endpoint{address, port},
        util::TagDecoratorFactory(config),
        dosGuard,
        handler,
        std::move(adminPassword)
    );

    server->run();
    return server;
}

}  // namespace web
