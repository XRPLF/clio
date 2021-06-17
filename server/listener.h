//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef LISTENER_H
#define LISTENER_H

#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <server/HttpSession.h>
#include <server/PlainWsSession.h>
#include <server/SslHttpSession.h>
#include <server/SslWsSession.h>
#include <server/SubscriptionManager.h>

#include <iostream>

class SubscriptionManager;

template <class PlainSession, class SslSession>
class Detector
    : public std::enable_shared_from_this<Detector<PlainSession, SslSession>>
{
    using std::enable_shared_from_this<
        Detector<PlainSession, SslSession>>::shared_from_this;

    boost::beast::tcp_stream stream_;
    std::optional<std::reference_wrapper<ssl::context>> ctx_;
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    DOSGuard& dosGuard_;
    boost::beast::flat_buffer buffer_;

public:
    Detector(
        tcp::socket&& socket,
        std::optional<std::reference_wrapper<ssl::context>> ctx,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard)
        : stream_(std::move(socket))
        , ctx_(ctx)
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
    {
    }

    // Launch the detector
    void
    run()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(stream_).expires_after(
            std::chrono::seconds(30));
        // Detect a TLS handshake
        async_detect_ssl(
            stream_,
            buffer_,
            boost::beast::bind_front_handler(
                &Detector::on_detect, shared_from_this()));
    }

    void
    on_detect(boost::beast::error_code ec, bool result)
    {
        if (ec)
            return httpFail(ec, "detect");

        if (result)
        {
            if (!ctx_)
                return httpFail(ec, "ssl not supported by this server");
            // Launch SSL session
            std::make_shared<SslSession>(
                stream_.release_socket(),
                *ctx_,
                backend_,
                subscriptions_,
                balancer_,
                dosGuard_,
                std::move(buffer_))
                ->run();
            return;
        }

        // Launch plain session
        std::make_shared<PlainSession>(
            stream_.release_socket(),
            backend_,
            subscriptions_,
            balancer_,
            dosGuard_,
            std::move(buffer_))
            ->run();
    }
};

template <class Body, class Allocator>
void
make_websocket_session(
    boost::beast::tcp_stream stream,
    http::request<Body, http::basic_fields<Allocator>> req,
    boost::beast::flat_buffer buffer,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<ETLLoadBalancer> balancer,
    DOSGuard& dosGuard)
{
    std::make_shared<WsUpgrader>(
        std::move(stream),
        backend,
        subscriptions,
        balancer,
        dosGuard,
        std::move(buffer))
        ->run();
}

template <class Body, class Allocator>
void
make_websocket_session(
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
    http::request<Body, http::basic_fields<Allocator>> req,
    boost::beast::flat_buffer buffer,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<ETLLoadBalancer> balancer,
    DOSGuard& dosGuard)
{
    std::make_shared<SslWsUpgrader>(
        std::move(stream),
        backend,
        subscriptions,
        balancer,
        dosGuard,
        std::move(buffer))
        ->run();
}
template <class PlainSession, class SslSession>
class Listener
    : public std::enable_shared_from_this<Listener<PlainSession, SslSession>>
{
    using std::enable_shared_from_this<
        Listener<PlainSession, SslSession>>::shared_from_this;

    net::io_context& ioc_;
    std::optional<std::reference_wrapper<ssl::context>> ctx_;
    tcp::acceptor acceptor_;
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    DOSGuard& dosGuard_;

public:
    Listener(
        net::io_context& ioc,
        std::optional<std::reference_wrapper<ssl::context>> ctx,
        tcp::endpoint endpoint,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard)
        : ioc_(ioc)
        , ctx_(ctx)
        , acceptor_(net::make_strand(ioc))
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
    {
        boost::beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            httpFail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
        {
            httpFail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            httpFail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            httpFail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        do_accept();
    }

private:
    void
    do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            boost::beast::bind_front_handler(
                &Listener::on_accept, shared_from_this()));
    }

    void
    on_accept(boost::beast::error_code ec, tcp::socket socket)
    {
        if (ec)
        {
            httpFail(ec, "listener_accept");
        }
        else
        {
            // Create the detector session and run it
            std::make_shared<Detector<PlainSession, SslSession>>(
                std::move(socket),
                ctx_,
                backend_,
                subscriptions_,
                balancer_,
                dosGuard_)
                ->run();
        }

        // Accept another connection
        do_accept();
    }
};

namespace Server {
using WebsocketServer = Listener<WsUpgrader, SslWsUpgrader>;
using HttpServer = Listener<HttpSession, SslHttpSession>;

static std::shared_ptr<WebsocketServer>
make_WebSocketServer(
    boost::json::object const& config,
    boost::asio::io_context& ioc,
    std::optional<std::reference_wrapper<ssl::context>> ctx,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<ETLLoadBalancer> balancer,
    DOSGuard& dosGuard)
{
    if (!config.contains("websocket_public"))
        return nullptr;

    auto const& wsConfig = config.at("websocket_public").as_object();

    auto const address =
        boost::asio::ip::make_address(wsConfig.at("ip").as_string().c_str());
    auto const port =
        static_cast<unsigned short>(wsConfig.at("port").as_int64());

    auto server = std::make_shared<WebsocketServer>(
        ioc,
        ctx,
        boost::asio::ip::tcp::endpoint{address, port},
        backend,
        subscriptions,
        balancer,
        dosGuard);

    server->run();
    return server;
}

static std::shared_ptr<HttpServer>
make_HttpServer(
    boost::json::object const& config,
    boost::asio::io_context& ioc,
    std::optional<std::reference_wrapper<ssl::context>> ctx,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<ETLLoadBalancer> balancer,
    DOSGuard& dosGuard)
{
    if (!config.contains("http_public"))
        return nullptr;

    auto const& httpConfig = config.at("http_public").as_object();

    auto const address =
        boost::asio::ip::make_address(httpConfig.at("ip").as_string().c_str());
    auto const port =
        static_cast<unsigned short>(httpConfig.at("port").as_int64());

    auto server = std::make_shared<HttpServer>(
        ioc,
        ctx,
        boost::asio::ip::tcp::endpoint{address, port},
        backend,
        subscriptions,
        balancer,
        dosGuard);

    server->run();
    return server;
}
}  // namespace Server

#endif  // LISTENER_H
