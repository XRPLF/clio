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
#include <reporting/server/HttpSession.h>
#include <reporting/server/SslHttpSession.h>
#include <reporting/server/WsSession.h>
#include <reporting/server/SslWsSession.h>
#include <reporting/server/SubscriptionManager.h>

#include <iostream>

class SubscriptionManager;

template <class PlainSession, class SslSession>
class Detector : public std::enable_shared_from_this<Detector<PlainSession, SslSession>>
{
    using std::enable_shared_from_this<Detector<PlainSession, SslSession>>::shared_from_this;

    boost::beast::tcp_stream stream_;
    ssl::context& ctx_;
    ReportingETL& etl_;
    boost::beast::flat_buffer buffer_;

public:
    Detector(
        tcp::socket&& socket,
        ssl::context& ctx,
        ReportingETL& etl)
        : stream_(std::move(socket))
        , ctx_(ctx)
        , etl_(etl)
    {
    }

    // Launch the detector
    void
    run()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
        // Detect a TLS handshake
        async_detect_ssl(
            stream_,
            buffer_,
            boost::beast::bind_front_handler(
                &Detector::on_detect,
                shared_from_this()));
    }

    void
    on_detect(boost::beast::error_code ec, bool result)
    {
        if(ec)
            return httpFail(ec, "detect");

        if(result)
        {
            // Launch SSL session
            std::make_shared<SslSession>(
                stream_.release_socket(),
                ctx_,
                etl_,
                std::move(buffer_))->run();
            return;
        }

        // Launch plain session
        std::make_shared<PlainSession>(
            stream_.release_socket(),
            etl_,
            std::move(buffer_))->run();
    }
};

template <class PlainSession, class SslSession>
class Listener : public std::enable_shared_from_this<Listener<PlainSession, SslSession>>
{
    using std::enable_shared_from_this<Listener<PlainSession, SslSession>>::shared_from_this;

    net::io_context& ioc_;
    ssl::context& ctx_;
    tcp::acceptor acceptor_;
    ReportingETL& etl_;

public:
    static void
    make_Listener(
        boost::asio::io_context& ioc,
        boost::asio::ip::tcp::endpoint endpoint,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer)
    {
        std::make_shared<listener>(
            ioc,
            endpoint,
            backend,
            subscriptions,
            balancer
        )->run();
    }

    Listener(
        boost::asio::io_context& ioc,
        boost::asio::ip::tcp::endpoint endpoint,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer)
        : ioc_(ioc)
        , acceptor_(ioc)
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
    {
        boost::beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            httpFail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            httpFail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            httpFail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if(ec)
        {
            httpFail(ec, "listen");
            return;
        }
    }

    ~listener() = default;

private:

    void
    run()
    {
        do_accept();
    }

    void
    do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            boost::beast::bind_front_handler(
                &Listener::on_accept,
                shared_from_this()));
    }

    void
    on_accept(boost::beast::error_code ec, tcp::socket socket)
    {
        if(ec)
        {
            httpFail(ec, "listener_accept");
        }
        else
        {
            // Create the detector session and run it
            std::make_shared<Detector<PlainSession, SslSession>>(
                std::move(socket),
                ctx_,
                etl_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

#endif // LISTENER_H