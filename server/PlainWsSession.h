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

#ifndef RIPPLE_REPORTING_WS_SESSION_H
#define RIPPLE_REPORTING_WS_SESSION_H

#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <etl/ReportingETL.h>
#include <server/Handlers.h>
#include <server/WsBase.h>
#include <server/listener.h>

#include <iostream>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

class ReportingETL;

// Echoes back all received WebSocket messages
class WsSession : public WsBase, public std::enable_shared_from_this<WsSession>
{
    websocket::stream<boost::beast::tcp_stream> ws_;
    boost::beast::flat_buffer buffer_;
    std::string response_;

    std::shared_ptr<BackendInterface> backend_;
    std::weak_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    DOSGuard& dosGuard_;

public:
    // Take ownership of the socket
    explicit WsSession(
        boost::asio::ip::tcp::socket&& socket,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard,
        boost::beast::flat_buffer buffer)
        : ws_(std::move(socket))
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
        , buffer_(std::move(buffer))
    {
    }

    ~WsSession() = default;

    void
    send(std::string&& msg)
    {
        ws_.text(ws_.got_text());
        ws_.async_write(
            boost::asio::buffer(msg),
            boost::beast::bind_front_handler(
                &WsSession::on_write, shared_from_this()));
    }

    void
    run(http::request<http::string_body> req)
    {
        std::cout << "Ran ws" << std::endl;
        // Set suggested timeout settings for the websocket
        ws_.set_option(websocket::stream_base::timeout::suggested(
            boost::beast::role_type::server));

        std::cout << "Trying to decorate" << std::endl;
        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(
                    http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-server-async");
            }));

        std::cout << "trying to async accept" << std::endl;
        ws_.async_accept(
            req,
            boost::beast::bind_front_handler(
                &WsSession::on_accept, shared_from_this()));
    }

private:
    void
    on_accept(boost::beast::error_code ec)
    {
        std::cout << "accepted WS" << std::endl;
        if (ec)
            return wsFail(ec, "acceptWS");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        // Read a message into our buffer

        std::cout << "doing read WS" << std::endl;
        ws_.async_read(
            buffer_,
            boost::beast::bind_front_handler(
                &WsSession::on_read, shared_from_this()));
        boost::beast::bind_front_handler(
            &WsSession::on_read, shared_from_this());
    }

    void
    on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed
        if (ec == boost::beast::websocket::error::closed)
            return;

        if (ec)
            wsFail(ec, "read");

        std::string msg{
            static_cast<char const*>(buffer_.data().data()), buffer_.size()};
        // BOOST_LOG_TRIVIAL(debug) << __func__ << msg;
        boost::json::object response;
        auto ip =
            ws_.next_layer().socket().remote_endpoint().address().to_string();
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " received request from ip = " << ip;
        if (!dosGuard_.isOk(ip))
            response["error"] = "Too many requests. Slow down";
        else
        {
            try
            {
                boost::json::value raw = boost::json::parse(msg);
                boost::json::object request = raw.as_object();
                BOOST_LOG_TRIVIAL(debug) << " received request : " << request;
                try
                {
                    std::shared_ptr<SubscriptionManager> subPtr =
                        subscriptions_.lock();
                    if (!subPtr)
                        return;

                    auto [res, cost] = buildResponse(
                        request,
                        backend_,
                        subPtr,
                        balancer_,
                        shared_from_this());
                    auto start = std::chrono::system_clock::now();
                    response = std::move(res);
                    if (!dosGuard_.add(ip, cost))
                    {
                        response["warning"] = "Too many requests";
                    }

                    auto end = std::chrono::system_clock::now();
                    BOOST_LOG_TRIVIAL(info)
                        << __func__ << " RPC call took "
                        << ((end - start).count() / 1000000000.0)
                        << " . request = " << request;
                }
                catch (Backend::DatabaseTimeout const& t)
                {
                    BOOST_LOG_TRIVIAL(error) << __func__ << " Database timeout";
                    response["error"] =
                        "Database read timeout. Please retry the request";
                }
            }
            catch (std::exception const& e)
            {
                BOOST_LOG_TRIVIAL(error)
                    << __func__ << "caught exception : " << e.what();
                response["error"] = "Unknown exception";
            }
        }
        BOOST_LOG_TRIVIAL(trace) << __func__ << response;
        response_ = boost::json::serialize(response);

        // Echo the message
        ws_.text(ws_.got_text());
        ws_.async_write(
            boost::asio::buffer(response_),
            boost::beast::bind_front_handler(
                &WsSession::on_write, shared_from_this()));
    }

    void
    on_write(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        std::cout << "writing WS" << std::endl;

        boost::ignore_unused(bytes_transferred);

        if (ec)
            return wsFail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};

class WsUpgrader : public std::enable_shared_from_this<WsUpgrader>
{
    boost::beast::tcp_stream http_;
    boost::optional<http::request_parser<http::string_body>> parser_;
    boost::beast::flat_buffer buffer_;
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    DOSGuard& dosGuard_;
    http::request<http::string_body> req_;

public:
    WsUpgrader(
        boost::asio::ip::tcp::socket&& socket,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard,
        boost::beast::flat_buffer&& b)
        : http_(std::move(socket))
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
        , buffer_(std::move(b))
    {
        std::cout << "1" << std::endl;
    }
    WsUpgrader(
        boost::beast::tcp_stream&& stream,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard,
        boost::beast::flat_buffer&& b,
        http::request<http::string_body> req)
        : http_(std::move(stream))
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
        , buffer_(std::move(b))
        , req_(std::move(req))
    {
        std::cout << "2" << std::endl;
    }

    void
    run()
    {
        std::cout << "RUNNING" << std::endl;
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.

        net::dispatch(
            http_.get_executor(),
            boost::beast::bind_front_handler(
                &WsUpgrader::do_upgrade, shared_from_this()));
    }

private:
    void
    do_upgrade()
    {
        std::cout << "doing upgrade" << std::endl;
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser_->body_limit(10000);

        // Set the timeout.
        boost::beast::get_lowest_layer(http_).expires_after(
            std::chrono::seconds(30));

        on_upgrade();
    }

    void
    on_upgrade()
    {
        // See if it is a WebSocket Upgrade
        if (!websocket::is_upgrade(req_))
        {
            std::cout << "is not upgrade" << std::endl;
            return;
        }

        // Disable the timeout.
        // The websocket::stream uses its own timeout settings.
        boost::beast::get_lowest_layer(http_).expires_never();

        std::cout << "making session" << std::endl;
        std::make_shared<WsSession>(
            http_.release_socket(),
            backend_,
            subscriptions_,
            balancer_,
            dosGuard_,
            std::move(buffer_))
            ->run(std::move(req_));
    }
};

#endif  // RIPPLE_REPORTING_WS_SESSION_H
