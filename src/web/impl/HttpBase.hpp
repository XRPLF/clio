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

#include "main/Build.hpp"
#include "rpc/Errors.hpp"
#include "util/Taggable.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Http.hpp"
#include "web/DOSGuard.hpp"
#include "web/impl/AdminVerificationStrategy.hpp"
#include "web/interface/Concepts.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/asio/error.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/json.hpp>
#include <boost/json/array.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <ripple/protocol/ErrorCodes.h>

#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace web::impl {

using tcp = boost::asio::ip::tcp;

/**
 * @brief This is the implementation class for http sessions
 *
 * @tparam Derived The derived class
 * @tparam HandlerType The handler class, will be called when a request is received.
 */
template <template <class> class Derived, SomeServerHandler HandlerType>
class HttpBase : public ConnectionBase {
    Derived<HandlerType>&
    derived()
    {
        return static_cast<Derived<HandlerType>&>(*this);
    }

    // TODO: this should be rewritten using http::message_generator instead
    struct SendLambda {
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

            // The lifetime of the message has to extend for the duration of the async operation so we use a shared_ptr
            // to manage it.
            auto sp = std::make_shared<http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.derived().stream(),
                *sp,
                boost::beast::bind_front_handler(&HttpBase::onWrite, self_.derived().shared_from_this(), sp->need_eof())
            );
        }
    };

    std::shared_ptr<void> res_;
    SendLambda sender_;
    std::shared_ptr<AdminVerificationStrategy> adminVerification_;

protected:
    boost::beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::reference_wrapper<web::DOSGuard> dosGuard_;
    std::shared_ptr<HandlerType> const handler_;
    util::Logger log_{"WebServer"};
    util::Logger perfLog_{"Performance"};

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

        if (!ec_ && ec != boost::asio::error::operation_aborted) {
            ec_ = ec;
            LOG(perfLog_.info()) << tag() << ": " << what << ": " << ec.message();
            boost::beast::get_lowest_layer(derived().stream()).socket().close(ec);
        }
    }

public:
    HttpBase(
        std::string const& ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::shared_ptr<AdminVerificationStrategy> adminVerification,
        std::reference_wrapper<web::DOSGuard> dosGuard,
        std::shared_ptr<HandlerType> handler,
        boost::beast::flat_buffer buffer
    )
        : ConnectionBase(tagFactory, ip)
        , sender_(*this)
        , adminVerification_(std::move(adminVerification))
        , buffer_(std::move(buffer))
        , dosGuard_(dosGuard)
        , handler_(std::move(handler))
    {
        LOG(perfLog_.debug()) << tag() << "http session created";
        dosGuard_.get().increment(ip);
    }

    ~HttpBase() override
    {
        LOG(perfLog_.debug()) << tag() << "http session closed";
        if (not upgraded)
            dosGuard_.get().decrement(this->clientIp);
    }

    void
    doRead()
    {
        if (dead())
            return;

        // Make the request empty before reading, otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        boost::beast::get_lowest_layer(derived().stream()).expires_after(std::chrono::seconds(30));

        http::async_read(
            derived().stream(),
            buffer_,
            req_,
            boost::beast::bind_front_handler(&HttpBase::onRead, derived().shared_from_this())
        );
    }

    void
    onRead(boost::beast::error_code ec, [[maybe_unused]] std::size_t bytes_transferred)
    {
        if (ec == http::error::end_of_stream)
            return derived().doClose();

        if (ec)
            return httpFail(ec, "read");

        // Update isAdmin property of the connection
        ConnectionBase::isAdmin = adminVerification_->isAdmin(req_, this->clientIp);

        if (boost::beast::websocket::is_upgrade(req_)) {
            if (dosGuard_.get().isOk(this->clientIp)) {
                // Disable the timeout. The websocket::stream uses its own timeout settings.
                boost::beast::get_lowest_layer(derived().stream()).expires_never();

                upgraded = true;
                return derived().upgrade();
            }

            return sender_(httpResponse(http::status::too_many_requests, "text/html", "Too many requests"));
        }

        if (auto response = util::prometheus::handlePrometheusRequest(req_, isAdmin); response.has_value())
            return sender_(std::move(response.value()));

        if (req_.method() != http::verb::post) {
            return sender_(httpResponse(http::status::bad_request, "text/html", "Expected a POST request"));
        }

        // to avoid overwhelm work queue, the request limit check should be
        // before posting to queue the web socket creation will be guarded via
        // connection limit
        if (!dosGuard_.get().request(clientIp)) {
            // TODO: this looks like it could be useful to count too in the future
            return sender_(httpResponse(
                http::status::service_unavailable,
                "text/plain",
                boost::json::serialize(rpc::makeError(rpc::RippledError::rpcSLOW_DOWN))
            ));
        }

        LOG(log_.info()) << tag() << "Received request from ip = " << clientIp << " - posting to WorkQueue";

        try {
            (*handler_)(req_.body(), derived().shared_from_this());
        } catch (std::exception const&) {
            return sender_(httpResponse(
                http::status::internal_server_error,
                "application/json",
                boost::json::serialize(rpc::makeError(rpc::RippledError::rpcINTERNAL))
            ));
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
        if (!dosGuard_.get().add(clientIp, msg.size())) {
            auto jsonResponse = boost::json::parse(msg).as_object();
            jsonResponse["warning"] = "load";
            if (jsonResponse.contains("warnings") && jsonResponse["warnings"].is_array()) {
                jsonResponse["warnings"].as_array().push_back(rpc::makeWarning(rpc::warnRPC_RATE_LIMIT));
            } else {
                jsonResponse["warnings"] = boost::json::array{rpc::makeWarning(rpc::warnRPC_RATE_LIMIT)};
            }

            // Reserialize when we need to include this warning
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

}  // namespace web::impl
