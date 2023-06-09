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
#include <webserver/interface/Concepts.h>
#include <webserver/interface/ConnectionBase.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>

#include <memory>
#include <string>

namespace Server {

using tcp = boost::asio::ip::tcp;

/**
 * This is the implementation class for http sessions
 * @tparam Derived The derived class
 * @tparam Handler The handler class, will be called when a request is received.
 */
template <template <class> class Derived, ServerHandler Handler>
class HttpBase : public ConnectionBase
{
    Derived<Handler>&
    derived()
    {
        return static_cast<Derived<Handler>&>(*this);
    }

    struct SendLambda
    {
        HttpBase& self_;

        explicit SendLambda(HttpBase& self) : self_(self)
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
    SendLambda sender_;

protected:
    boost::beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::reference_wrapper<clio::DOSGuard> dosGuard_;
    std::shared_ptr<Handler> const handler_;
    clio::Logger log_{"WebServer"};
    clio::Logger perfLog_{"Performance"};

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
            perfLog_.info() << tag() << ": " << what << ": " << ec.message();
            boost::beast::get_lowest_layer(derived().stream()).socket().close(ec);
        }
    }

public:
    HttpBase(
        std::string const& ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<clio::DOSGuard> dosGuard,
        std::shared_ptr<Handler> const& handler,
        boost::beast::flat_buffer buffer)
        : ConnectionBase(tagFactory, ip)
        , sender_(*this)
        , buffer_(std::move(buffer))
        , dosGuard_(dosGuard)
        , handler_(handler)
    {
        perfLog_.debug() << tag() << "http session created";
        dosGuard_.get().increment(ip);
    }

    virtual ~HttpBase()
    {
        perfLog_.debug() << tag() << "http session closed";
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

        http::async_read(
            derived().stream(),
            buffer_,
            req_,
            boost::beast::bind_front_handler(&HttpBase::onRead, derived().shared_from_this()));
    }

    void
    onRead(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

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
            return sender_(httpResponse(http::status::bad_request, "text/html", "Expected a POST request"));
        }

        // to avoid overwhelm work queue, the request limit check should be
        // before posting to queue the web socket creation will be guarded via
        // connection limit
        if (!dosGuard_.get().request(clientIp))
        {
            return sender_(httpResponse(
                http::status::service_unavailable,
                "text/plain",
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcSLOW_DOWN))));
        }

        log_.info() << tag() << "Received request from ip = " << clientIp << " - posting to WorkQueue";

        try
        {
            (*handler_)(req_.body(), derived().shared_from_this());
        }
        catch (std::exception const& e)
        {
            perfLog_.error() << tag() << "Caught exception : " << e.what();
            return sender_(httpResponse(
                http::status::internal_server_error,
                "application/json",
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcINTERNAL))));
        }
    }

    /**
     * @brief Send a response to the client
     * The message length will be added to the DOSGuard, if the limit is reached, a warning will be added to the
     * response
     */
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
        sender_(httpResponse(status, "application/json", std::move(msg)));
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

        doRead();
    }

private:
    http::response<http::string_body>
    httpResponse(http::status status, std::string content_type, std::string message) const
    {
        http::response<http::string_body> res{status, req_.version()};
        res.set(http::field::server, "clio-server-" + Build::getClioVersionString());
        res.set(http::field::content_type, content_type);
        res.keep_alive(req_.keep_alive());
        res.body() = std::move(message);
        res.prepare_payload();
        return res;
    };
};

}  // namespace Server
