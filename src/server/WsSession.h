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
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <rpc/Handlers.h>
#include <server/WsBase.h>
#include <reporting/ReportingETL.h>

#include <iostream>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;       
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

class ReportingETL;

// Echoes back all received WebSocket messages
class WsSession : public WsBase
                , public std::enable_shared_from_this<WsSession>
{
    websocket::stream<boost::beast::tcp_stream> ws_;
    boost::beast::flat_buffer buffer_;
    std::string response_;

    std::shared_ptr<BackendInterface> backend_;
    std::weak_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;

public:
    // Take ownership of the socket
    explicit WsSession(
        boost::asio::ip::tcp::socket&& socket,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer)
        : ws_(std::move(socket))
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
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
    close(boost::beast::websocket::close_reason const& cr)
    {
        ws_.async_close(
            cr, 
            boost::beast::bind_front_handler(
                &WsSession::on_close, shared_from_this()));
    }

    void
    run(http::request<http::string_body> req)
    {
        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                boost::beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res)
            {
                res.set(http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-server-async");
            }));

        ws_.async_accept(
            req,
            boost::beast::bind_front_handler(
                &WsSession::on_accept,
                shared_from_this()));
    }

private:
    void
    on_accept(boost::beast::error_code ec)
    {
        if (ec)
            return wsFail(ec, "acceptWS");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        // Read a message into our buffer

        ws_.async_read(
            buffer_,
            boost::beast::bind_front_handler(
                &WsSession::on_read, shared_from_this()));
    }

    void
    do_close();

    void
    on_close(boost::beast::error_code ec)
    {
        if (ec == boost::beast::websocket::error::closed
        || ec == boost::asio::error::operation_aborted)
            return;

        if (ec)
            return wsFail(ec, "close");

        do_close();
    }

    void
    on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed or cancelled
        if (ec == boost::beast::websocket::error::closed
            || ec == boost::asio::error::operation_aborted)
        {
            return do_close();
        }

        if (ec)
            wsFail(ec, "read");

        std::string msg{
            static_cast<char const*>(buffer_.data().data()), buffer_.size()};
        try
        {
            boost::json::value raw = boost::json::parse(msg);
            boost::json::object request = raw.as_object();
            BOOST_LOG_TRIVIAL(debug) << " received request : " << request;
            try
            {
                auto range = backend_->fetchLedgerRange();
                if (!range)
                    return send(boost::json::serialize(RPC::make_error(RPC::Error::rpcNOT_READY)));

                std::optional<RPC::Context> context = RPC::make_WsContext(
                    request,
                    backend_,
                    subscriptions_.lock(),
                    balancer_,
                    shared_from_this(),
                    *range
                );

                if (!context)
                    return send(boost::json::serialize(RPC::make_error(RPC::Error::rpcBAD_SYNTAX)));

                auto id = request.contains("id") 
                    ? request.at("id") 
                    : nullptr;

                auto response = getDefaultWsResponse(id);
                boost::json::object& result = response["result"].as_object();

                auto status = RPC::buildResponse(*context, result);

                if(status)
                {
                    auto error = RPC::make_error(status.error);

                    if (!id.is_null())
                        error["id"] = id;
                    error["request"] = request;

                    response_ = boost::json::serialize(error);

                }
                else
                {
                    response_ = boost::json::serialize(response);
                }
                
            }
            catch (Backend::DatabaseTimeout const& t)
            {
                BOOST_LOG_TRIVIAL(error) << __func__ << " Database timeout";
                return send(boost::json::serialize(RPC::make_error(RPC::Error::rpcNOT_READY)));
            }
        }
        catch (std::exception const& e)
        {
            BOOST_LOG_TRIVIAL(error)
                << __func__ << " caught exception : " << e.what();
        }
        BOOST_LOG_TRIVIAL(trace) << __func__ << response_;

        send(std::move(response_));
    }

    void
    on_write(boost::beast::error_code const& ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // Indicates the session is closed or  canceled
        if (ec == boost::beast::websocket::error::closed
            || ec == boost::asio::error::operation_aborted)
            return do_close();

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

public:
    WsUpgrader(
        boost::asio::ip::tcp::socket&& socket,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        boost::beast::flat_buffer&& b)
        : http_(std::move(socket))
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , buffer_(std::move(b))
        {}

    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.

        net::dispatch(
            http_.get_executor(),
            boost::beast::bind_front_handler(
                &WsUpgrader::do_upgrade,
                shared_from_this()));
    }

private:
    void
    do_upgrade()
    {
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser_->body_limit(10000);

        // Set the timeout.
        boost::beast::get_lowest_layer(http_).expires_after(std::chrono::seconds(30));

        // Read a request using the parser-oriented interface
        http::async_read(
            http_,
            buffer_,
            *parser_,
            boost::beast::bind_front_handler(
                &WsUpgrader::on_upgrade,
                shared_from_this()));
    }

    void
    on_upgrade(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return;

        if (ec)
            return wsFail(ec, "upgrade");

        // See if it is a WebSocket Upgrade
        if(!websocket::is_upgrade(parser_->get()))
            return wsFail(ec, "is_upgrade");

        // Disable the timeout.
        // The websocket::stream uses its own timeout settings.
        boost::beast::get_lowest_layer(http_).expires_never();

        std::make_shared<WsSession>(
            http_.release_socket(),
            backend_,
            subscriptions_,
            balancer_)->run(parser_->release());
    }
};

#endif // RIPPLE_REPORTING_WS_SESSION_H