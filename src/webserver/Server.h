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

#include <log/Logger.h>
#include <webserver/HttpSession.h>
#include <webserver/SslHttpSession.h>
#include <webserver/interface/Concepts.h>

#include <fmt/core.h>

namespace Server {

/**
 * @brief The Detector class to detect if the connection is a ssl or not.
 * If it is a ssl connection, it will pass the ownership of the socket to SslSession, otherwise to PlainSession.
 * @tparam PlainSession The plain session type
 * @tparam SslSession The ssl session type
 * @tparam Handler The executor to handle the requests
 */
template <template <class> class PlainSession, template <class> class SslSession, ServerHandler Handler>
class Detector : public std::enable_shared_from_this<Detector<PlainSession, SslSession, Handler>>
{
    using std::enable_shared_from_this<Detector<PlainSession, SslSession, Handler>>::shared_from_this;

    clio::Logger log_{"WebServer"};
    boost::beast::tcp_stream stream_;
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> ctx_;
    std::reference_wrapper<util::TagDecoratorFactory const> tagFactory_;
    std::reference_wrapper<clio::DOSGuard> const dosGuard_;
    std::shared_ptr<Handler> const handler_;
    boost::beast::flat_buffer buffer_;

public:
    Detector(
        tcp::socket&& socket,
        std::optional<std::reference_wrapper<boost::asio::ssl::context>> ctx,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<clio::DOSGuard> dosGuard,
        std::shared_ptr<Handler> const& handler)
        : stream_(std::move(socket))
        , ctx_(ctx)
        , tagFactory_(std::cref(tagFactory))
        , dosGuard_(dosGuard)
        , handler_(handler)
    {
    }

    inline void
    fail(boost::system::error_code ec, char const* message)
    {
        if (ec == boost::asio::ssl::error::stream_truncated)
            return;

        log_.info() << "Detector failed (" << message << "): " << ec.message();
    }

    void
    run()
    {
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
        async_detect_ssl(stream_, buffer_, boost::beast::bind_front_handler(&Detector::onDetect, shared_from_this()));
    }

    void
    onDetect(boost::beast::error_code ec, bool result)
    {
        if (ec)
            return fail(ec, "detect");

        std::string ip;
        try
        {
            ip = stream_.socket().remote_endpoint().address().to_string();
        }
        catch (std::exception const&)
        {
            return fail(ec, "cannot get remote endpoint");
        }

        if (result)
        {
            if (!ctx_)
                return fail(ec, "SSL is not supported by this server");

            std::make_shared<SslSession<Handler>>(
                stream_.release_socket(), ip, *ctx_, tagFactory_, dosGuard_, handler_, std::move(buffer_))
                ->run();
            return;
        }

        std::make_shared<PlainSession<Handler>>(
            stream_.release_socket(), ip, tagFactory_, dosGuard_, handler_, std::move(buffer_))
            ->run();
    }
};

/**
 * @brief The WebServer class. It creates server socket and start listening on it.
 * Once there is client connection, it will accept it and pass the socket to Detector to detect ssl or plain.
 * @tparam PlainSession The plain session to handler non-ssl connection.
 * @tparam SslSession The ssl session to handler ssl connection.
 * @tparam Handler The handler to process the request and return response.
 */
template <template <class> class PlainSession, template <class> class SslSession, ServerHandler Handler>
class Server : public std::enable_shared_from_this<Server<PlainSession, SslSession, Handler>>
{
    using std::enable_shared_from_this<Server<PlainSession, SslSession, Handler>>::shared_from_this;

    clio::Logger log_{"WebServer"};
    std::reference_wrapper<boost::asio::io_context> ioc_;
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> ctx_;
    util::TagDecoratorFactory tagFactory_;
    std::reference_wrapper<clio::DOSGuard> dosGuard_;
    std::shared_ptr<Handler> handler_;
    tcp::acceptor acceptor_;

public:
    Server(
        boost::asio::io_context& ioc,
        std::optional<std::reference_wrapper<boost::asio::ssl::context>> ctx,
        tcp::endpoint endpoint,
        util::TagDecoratorFactory tagFactory,
        clio::DOSGuard& dosGuard,
        std::shared_ptr<Handler> const& callback)
        : ioc_(std::ref(ioc))
        , ctx_(ctx)
        , tagFactory_(std::move(tagFactory))
        , dosGuard_(std::ref(dosGuard))
        , handler_(callback)
        , acceptor_(boost::asio::make_strand(ioc))
    {
        boost::beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
            return;

        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec)
            return;

        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            log_.error() << "Failed to bind to endpoint: " << endpoint << ". message: " << ec.message();
            throw std::runtime_error(
                fmt::format("Failed to bind to endpoint: {}:{}", endpoint.address().to_string(), endpoint.port()));
        }

        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
        {
            log_.error() << "Failed to listen at endpoint: " << endpoint << ". message: " << ec.message();
            throw std::runtime_error(
                fmt::format("Failed to listen at endpoint: {}:{}", endpoint.address().to_string(), endpoint.port()));
        }
    }

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
            boost::beast::bind_front_handler(&Server::onAccept, shared_from_this()));
    }

    void
    onAccept(boost::beast::error_code ec, tcp::socket socket)
    {
        if (!ec)
        {
            auto ctxRef =
                ctx_ ? std::optional<std::reference_wrapper<boost::asio::ssl::context>>{ctx_.value()} : std::nullopt;

            std::make_shared<Detector<PlainSession, SslSession, Handler>>(
                std::move(socket), ctxRef, std::cref(tagFactory_), dosGuard_, handler_)
                ->run();
        }

        doAccept();
    }
};

template <class Executor>
using HttpServer = Server<HttpSession, SslHttpSession, Executor>;

/**
 * @brief Create a http server.
 * @tparam Executor The executor to process the request.
 * @param config The config to create server.
 * @param ioc The server will run under this io_context.
 * @param sslCtx The ssl context to create ssl session.
 * @param dosGuard The dos guard to protect the server.
 * @param handler The executor to process the request.
 */
template <class Executor>
static std::shared_ptr<HttpServer<Executor>>
make_HttpServer(
    clio::Config const& config,
    boost::asio::io_context& ioc,
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> const& sslCtx,
    clio::DOSGuard& dosGuard,
    std::shared_ptr<Executor> const& handler)
{
    static clio::Logger log{"WebServer"};
    if (!config.contains("server"))
        return nullptr;

    auto const serverConfig = config.section("server");
    auto const address = boost::asio::ip::make_address(serverConfig.value<std::string>("ip"));
    auto const port = serverConfig.value<unsigned short>("port");

    auto server = std::make_shared<HttpServer<Executor>>(
        ioc,
        sslCtx,
        boost::asio::ip::tcp::endpoint{address, port},
        util::TagDecoratorFactory(config),
        dosGuard,
        handler);

    server->run();
    return server;
}

}  // namespace Server
