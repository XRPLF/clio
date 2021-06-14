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

#include <server/SubscriptionManager.h>

#include <iostream>

class SubscriptionManager;

// Detects SSL handshakes
class detect_session : public std::enable_shared_from_this<detect_session>
{
    boost::beast::tcp_stream stream_;
    std::optional<ssl::context>& ctx_;
    ReportingETL& etl_;
    boost::beast::flat_buffer buffer_;

public:
    detect_session(
        tcp::socket&& socket,
        std::optional<ssl::context>& ctx,
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

        if (!ctx_)
        {
            // Launch plain session
            std::make_shared<HttpSession>(
                stream_.release_socket(),
                etl_,
                std::move(buffer_))->run();
        }

        // Detect a TLS handshake
        async_detect_ssl(
            stream_,
            buffer_,
            boost::beast::bind_front_handler(
                &detect_session::on_detect,
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
            std::make_shared<SslHttpSession>(
                stream_.release_socket(),
                *ctx_,
                etl_,
                std::move(buffer_))->run();
            return;
        }

        // Launch plain session
        std::make_shared<HttpSession>(
            stream_.release_socket(),
            etl_,
            std::move(buffer_))->run();
    }
};

// Accepts incoming connections and launches the sessions
class Listener : public std::enable_shared_from_this<Listener>
{
    net::io_context& ioc_;
    std::optional<ssl::context>& ctx_;
    tcp::acceptor acceptor_;
    ReportingETL& etl_;

public:
    Listener(
        net::io_context& ioc,
        std::optional<ssl::context>& ctx,
        tcp::endpoint endpoint,
        ReportingETL& etl)
        : ioc_(ioc)
        , ctx_(ctx)
        , acceptor_(net::make_strand(ioc))
        , etl_(etl)
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
            httpFail(ec, "accept");
        }
        else
        {
            // Create the detector session and run it
            std::make_shared<detect_session>(
                std::move(socket),
                ctx_,
                etl_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

#endif  // LISTENER_H
