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
#include <main/Build.h>
#include <webserver/DOSGuard.h>
#include <webserver2/interface/Concepts.h>
#include <webserver2/interface/ConnectionBase.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>

#include <memory>
#include <string>

namespace Server {

using tcp = boost::asio::ip::tcp;

template <template <class> class Derived, ServerCallback Callback>
class HttpBase : public ConnectionBase
{
    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived<Callback>&
    derived()
    {
        return static_cast<Derived<Callback>&>(*this);
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
            if (self_.dead())
                return;

            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.derived().stream(),
                *sp,
                boost::beast::bind_front_handler(
                    &HttpBase::onWrite, self_.derived().shared_from_this(), sp->need_eof()));
        }
    };

    std::shared_ptr<void> res_;
    send_lambda lambda_;

protected:
    boost::beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::reference_wrapper<clio::DOSGuard> dosGuard_;
    std::reference_wrapper<util::TagDecoratorFactory const> tagFactory_;
    std::shared_ptr<Callback> const callback_;
    clio::Logger log{"WebServer"};
    clio::Logger perfLog{"Performance"};

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

        if (ec == boost::asio::ssl::error::stream_truncated)
            return;

        if (!ec_ && ec != boost::asio::error::operation_aborted)
        {
            ec_ = ec;
            perfLog.info() << tag() << ": " << what << ": " << ec.message();
            boost::beast::get_lowest_layer(derived().stream()).socket().close(ec);
        }
    }

public:
    HttpBase(
        std::string const& ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<clio::DOSGuard> dosGuard,
        std::shared_ptr<Callback> const& callback,
        boost::beast::flat_buffer buffer)
        : ConnectionBase(tagFactory, ip)
        , lambda_(*this)
        , buffer_(std::move(buffer))
        , dosGuard_(dosGuard)
        , tagFactory_(tagFactory)
        , callback_(callback)
    {
        perfLog.debug() << tag() << "http session created";
        dosGuard_.get().increment(ip);
    }

    virtual ~HttpBase()
    {
        perfLog.debug() << tag() << "http session closed";
        if (not upgraded)
            dosGuard_.get().decrement(this->clientIp);
    }

    void
    doRead()
    {
        if (dead())
            return;
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        boost::beast::get_lowest_layer(derived().stream()).expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(
            derived().stream(),
            buffer_,
            req_,
            boost::beast::bind_front_handler(&HttpBase::onRead, derived().shared_from_this()));
    }

    http::response<http::string_body>
    httpResponse(http::status status, std::string content_type, std::string message) const
    {
        http::response<http::string_body> res{status, req_.version()};
        res.set(http::field::server, "clio-server-" + Build::getClioVersionString());
        res.set(http::field::content_type, content_type);
        res.keep_alive(req_.keep_alive());
        res.body() = std::string(message);
        res.prepare_payload();
        return res;
    };

    void
    onRead(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == http::error::end_of_stream)
            return derived().doClose();

        if (ec)
            return httpFail(ec, "read");

        if (boost::beast::websocket::is_upgrade(req_))
        {
            upgraded = true;
            // Disable the timeout.
            // The websocket::stream uses its own timeout settings.
            boost::beast::get_lowest_layer(derived().stream()).expires_never();
            return derived().upgrade();
        }

        if (req_.method() != http::verb::post)
        {
            return lambda_(httpResponse(http::status::bad_request, "text/html", "Expected a POST request"));
        }

        // to avoid overwhelm work queue, the request limit check should be
        // before posting to queue the web socket creation will be guarded via
        // connection limit
        if (!dosGuard_.get().request(clientIp))
        {
            return lambda_(httpResponse(
                http::status::service_unavailable,
                "text/plain",
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcSLOW_DOWN))));
        }

        log.info() << tag() << "Received request from ip = " << clientIp << " - posting to WorkQueue";

        auto request = boost::json::object{};
        try
        {
            request = boost::json::parse(req_.body()).as_object();
        }
        catch (boost::exception const& e)
        {
            return lambda_(httpResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcBAD_SYNTAX))));
        }
        try
        {
            (*callback_)(std::move(request), derived().shared_from_this());
        }
        catch (std::exception const& e)
        {
            perfLog.error() << tag() << "Caught exception : " << e.what();
            return lambda_(httpResponse(
                http::status::internal_server_error,
                "application/json",
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcINTERNAL))));
        }
    }

    void
    send(std::string&& msg, http::status status = http::status::ok) override
    {
        if (!dosGuard_.get().add(clientIp, msg.size()))
        {
            auto jsonResponse = boost::json::parse(msg).as_object();
            jsonResponse["warning"] = "load";
            if (jsonResponse.contains("warnings") && jsonResponse["warnings"].is_array())
                jsonResponse["warnings"].as_array().push_back(RPC::makeWarning(RPC::warnRPC_RATE_LIMIT));
            else
                jsonResponse["warnings"] = boost::json::array{RPC::makeWarning(RPC::warnRPC_RATE_LIMIT)};
            // reserialize when we need to include this warning
            msg = boost::json::serialize(jsonResponse);
        }
        lambda_(httpResponse(status, "application/json", std::move(msg)));
    }

    void
    onWrite(bool close, boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return httpFail(ec, "write");

        // This means we should close the connection, usually because
        // the response indicated the "Connection: close" semantic.
        if (close)
            return derived().doClose();

        // We're done with the response so delete it
        res_ = nullptr;
        // Read another request
        doRead();
    }
};

}  // namespace Server
