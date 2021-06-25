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

#ifndef RIPPLE_REPORTING_HTTP_BASE_SESSION_H
#define RIPPLE_REPORTING_HTTP_BASE_SESSION_H

#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <boost/json.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <handlers/Handlers.h>
#include <webserver/DOSGuard.h>
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

    if (ec == net::ssl::error::stream_truncated)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <class Body, class Allocator, class Send>
void
handle_request(
    boost::beast::http::
        request<Body, boost::beast::http::basic_fields<Allocator>>&& req,
    Send&& send,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<ETLLoadBalancer> balancer,
    DOSGuard& dosGuard,
    std::string const& ip)
{
    auto const response = [&req](
                              http::status status,
                              std::string content_type,
                              std::string message) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::server, "xrpl-reporting-server-v0.0.0");
        res.set(http::field::content_type, content_type);
        res.keep_alive(req.keep_alive());
        res.body() = std::string(message);
        res.prepare_payload();
        return res;
    };

    if (req.method() == http::verb::get && req.body() == "")
    {
        send(response(http::status::ok, "text/html", defaultResponse));
        return;
    }

    if (req.method() != http::verb::post)
    {
        send(response(
            http::status::bad_request, "text/html", "Expected a POST request"));

    if (!dosGuard.isOk(ip))
        return send(httpResponse(
            http::status::ok,
            "application/json",
            boost::json::serialize(
                RPC::make_error(RPC::Error::rpcSLOW_DOWN))));

    try
    {
        BOOST_LOG_TRIVIAL(info) << "Received request: " << req.body();

        boost::json::object request;
        try
        {
            request = boost::json::parse(req.body()).as_object();
        }
        catch (std::runtime_error const& e)
        {
            return send(httpResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(
                    RPC::make_error(RPC::Error::rpcBAD_SYNTAX))));
        }

        if (!dosGuard.isOk(ip))
            return send(httpResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(
                    RPC::make_error(RPC::Error::rpcSLOW_DOWN))));

        auto range = backend->fetchLedgerRange();
        if (!range)
            return send(httpResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(
                    RPC::make_error(RPC::Error::rpcNOT_READY))));
                
        std::optional<RPC::Context> context = RPC::make_HttpContext(
            request,
            backend,
            nullptr,
            balancer,
            *range
        );

        if (!context)
            return send(httpResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(
                    RPC::make_error(RPC::Error::rpcBAD_SYNTAX))));
    
        boost::json::object response{{"result", boost::json::object{}}};
        boost::json::object& result = response["result"].as_object();

        auto [status, cost] = RPC::buildResponse(*context, result);

        if (!dosGuard.add(ip, cost))
            result["warning"] = "Too many requests";

        if(status)
        {
            auto error = RPC::make_error(status.error);

            error["request"] = request;

            result = error;
        }
        else
        {
            result["status"] = "success";
            result["validated"] = true;
        }

        return send(httpResponse(
            http::status::ok,
            "application/json",
            boost::json::serialize(response)));
    }
    catch (std::exception const& e)
    {
        return send(httpResponse(
            http::status::internal_server_error,
            "application/json",
            boost::json::serialize(RPC::make_error(RPC::Error::rpcINTERNAL))));
    }
}

// From Boost Beast examples http_server_flex.cpp
template <class Derived>
class HttpBase
{
    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }

    struct send_lambda
    {
        HttpBase& self_;

        explicit send_lambda(HttpBase& self) : self_(self)
        {
        }

        template <bool isRequest, class Body, class Fields>
        void
        operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<http::message<isRequest, Body, Fields>>(
                std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.derived().stream(),
                *sp,
                boost::beast::bind_front_handler(
                    &HttpBase::on_write,
                    self_.derived().shared_from_this(),
                    sp->need_eof()));
        }
    };

    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    DOSGuard& dosGuard_;
    send_lambda lambda_;

protected:
    boost::beast::flat_buffer buffer_;

public:
    HttpBase(
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard,
        boost::beast::flat_buffer buffer)
        : backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
        , lambda_(*this)
        , buffer_(std::move(buffer))
    {
    }

    void
    do_read()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        boost::beast::get_lowest_layer(derived().stream())
            .expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(
            derived().stream(),
            buffer_,
            req_,
            boost::beast::bind_front_handler(
                &HttpBase::on_read, derived().shared_from_this()));
    }

    void
    on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == http::error::end_of_stream)
            return derived().do_close();

        if (ec)
            return httpFail(ec, "read");

        if (boost::beast::websocket::is_upgrade(req_))
        {
            // Disable the timeout.
            // The websocket::stream uses its own timeout settings.
            boost::beast::get_lowest_layer(derived().stream()).expires_never();
            return make_websocket_session(
                derived().release_stream(),
                std::move(req_),
                std::move(buffer_),
                backend_,
                subscriptions_,
                balancer_,
                dosGuard_);
        }

        auto ip = derived().ip();

        // Send the response
        handle_request(
            std::move(req_), lambda_, backend_, balancer_, dosGuard_, ip);
    }

    void
    on_write(
        bool close,
        boost::beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return httpFail(ec, "write");

        if (close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return derived().do_close();
        }

        // We're done with the response so delete it
        res_ = nullptr;

        // Read another request
        do_read();
    }
};

#endif  // RIPPLE_REPORTING_HTTP_BASE_SESSION_H
