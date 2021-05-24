//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2021 Ripple Labs Inc.

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

#ifndef RIPPLE_REPORTING_HTTP_SESSION_H
#define RIPPLE_REPORTING_HTTP_SESSION_H

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;       
using tcp = boost::asio::ip::tcp;

static std::string defaultResponse = 
    "<!DOCTYPE html><html><head><title>"
    " Test page for reporting mode</title></head><body><h1>"
    " Test</h1><p>This page shows xrpl reporting http(s) "
    "connectivity is working.</p></body></html>";

inline void
httpFail(boost::beast::error_code ec, char const* what)
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

    if(ec == net::ssl::error::stream_truncated)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
    class Body, class Allocator,
    class Send>
void
handle_request(
    boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>&& req,
    Send&& send,
    ReportingETL& etl)
{

    auto const response =
    [&req](
        http::status status,
        std::string content_type,
        std::string message)
    {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::server, "xrpl-reporting-server-v0.0.0");
        res.set(http::field::content_type, content_type);
        res.keep_alive(req.keep_alive());
        res.body() = std::string(message);
        res.prepare_payload();
        return res;
    };


    if(req.method() == http::verb::get
      && req.body() == "")
    {
        send(response(http::status::ok, "text/html", defaultResponse));
        return;
    }

    if(req.method() != http::verb::post)
    {
        send(response(
            http::status::bad_request, 
            "text/html", 
            "Expected a POST request"));

        return;
    }

    try 
    {
        std::cout << "GOT BODY: " << req.body() << std::endl;
        auto request = boost::json::parse(req.body()).as_object();

        std::cout << "GOT REQUEST: " << request << std::endl;

        auto builtResponse = buildResponse(request, etl, nullptr);

        send(response(
            http::status::ok,
            "application/json",
            boost::json::serialize(builtResponse)));

        return;
    }
    catch (std::exception const& e)
    {
        std::cout << e.what() << std::endl;
        send(response(
            http::status::internal_server_error,
            "text/html",
            "Internal server error occurred"
        ));

        return;
    }
}


// Handles an HTTP server connection
class HttpSession : public std::enable_shared_from_this<HttpSession>
{
    struct send_lambda
    {
        HttpSession& self_;

        explicit
        send_lambda(HttpSession& self)
            : self_(self)
        {
        }

        template<bool isRequest, class Body, class Fields>
        void
        operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<
                http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.stream_,
                *sp,
                boost::beast::bind_front_handler(
                    &HttpSession::on_write,
                    self_.shared_from_this(),
                    sp->need_eof()));
        }
    };

    boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
    boost::beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    send_lambda lambda_;
    ReportingETL& etl_;

public:
    // Take ownership of the socket
    explicit
    HttpSession(
        tcp::socket&& socket,
        ssl::context& ctx,
        ReportingETL& etl)
        : stream_(std::move(socket), ctx)
        , lambda_(*this)
        , etl_(etl)
    {
    }

    // Start the asynchronous operation
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this HttpSession. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(
            stream_.get_executor(),
            boost::beast::bind_front_handler(
                &HttpSession::on_run,
                shared_from_this()));
    }

    void
    on_run()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(stream_).expires_after(
            std::chrono::seconds(30));

        // Perform the SSL handshake
        stream_.async_handshake(
            ssl::stream_base::server,
            boost::beast::bind_front_handler(
                &HttpSession::on_handshake,
                shared_from_this()));
    }

    void
    on_handshake(boost::beast::error_code ec)
    {
        if(ec)
            return httpFail(ec, "handshake");

        do_read();
    }

    void
    do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(stream_, buffer_, req_,
            boost::beast::bind_front_handler(
                &HttpSession::on_read,
                shared_from_this()));
    }

    void
    on_read(
        boost::beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return do_close();

        if(ec)
            return httpFail(ec, "read");

        // Send the response
        handle_request(std::move(req_), lambda_, etl_);
    }

    void
    on_write(
        bool close,
        boost::beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return httpFail(ec, "write");

        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // We're done with the response so delete it
        res_ = nullptr;

        // Read another request
        do_read();
    }

    void
    do_close()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Perform the SSL shutdown
        stream_.async_shutdown(
            boost::beast::bind_front_handler(
                &HttpSession::on_shutdown,
                shared_from_this()));
    }

    void
    on_shutdown(boost::beast::error_code ec)
    {
        if(ec)
            return httpFail(ec, "shutdown");

        // At this point the connection is closed gracefully
    }
};

#endif // RIPPLE_REPORTING_HTTP_SESSION_H