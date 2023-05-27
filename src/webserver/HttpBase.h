//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <boost/asio/dispatch.hpp>
#include <boost/asio/spawn.hpp>
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

#include <etl/ReportingETL.h>
#include <log/Logger.h>
#include <main/Build.h>
#include <rpc/Counters.h>
#include <rpc/Factories.h>
#include <rpc/RPCEngine.h>
#include <rpc/WorkQueue.h>
#include <util/Profiler.h>
#include <util/Taggable.h>
#include <vector>
#include <webserver/DOSGuard.h>

// TODO: consider removing those - visible to anyone including this header
namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

static std::string defaultResponse =
    "<!DOCTYPE html><html><head><title>"
    " Test page for reporting mode</title></head><body><h1>"
    " Test</h1><p>This page shows xrpl reporting http(s) "
    "connectivity is working.</p></body></html>";

// From Boost Beast examples http_server_flex.cpp
template <class Derived>
class HttpBase : public util::Taggable
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

    boost::system::error_code ec_;
    boost::asio::io_context& ioc_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    std::shared_ptr<BackendInterface const> backend_;
    std::shared_ptr<RPC::RPCEngine> rpcEngine_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    std::shared_ptr<ReportingETL const> etl_;
    util::TagDecoratorFactory const& tagFactory_;
    clio::DOSGuard& dosGuard_;
    send_lambda lambda_;

protected:
    clio::Logger log_{"WebServer"};
    clio::Logger perfLog_{"Performance"};
    boost::beast::flat_buffer buffer_;
    bool upgraded_ = false;

    bool
    dead()
    {
        return ec_ != boost::system::error_code{};
    }

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

        if (!ec_ && ec != boost::asio::error::operation_aborted)
        {
            ec_ = ec;
            perfLog_.info() << tag() << ": " << what << ": " << ec.message();
            boost::beast::get_lowest_layer(derived().stream()).socket().close(ec);
        }
    }

public:
    HttpBase(
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<RPC::RPCEngine> rpcEngine,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        std::shared_ptr<ReportingETL const> etl,
        util::TagDecoratorFactory const& tagFactory,
        clio::DOSGuard& dosGuard,
        boost::beast::flat_buffer buffer)
        : Taggable(tagFactory)
        , ioc_(ioc)
        , backend_(backend)
        , rpcEngine_(rpcEngine)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , etl_(etl)
        , tagFactory_(tagFactory)
        , dosGuard_(dosGuard)
        , lambda_(*this)
        , buffer_(std::move(buffer))
    {
        perfLog_.debug() << tag() << "http session created";
    }

    virtual ~HttpBase()
    {
        perfLog_.debug() << tag() << "http session closed";
    }

    clio::DOSGuard&
    dosGuard()
    {
        return dosGuard_;
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

    void
    onRead(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == http::error::end_of_stream)
            return derived().doClose();

        if (ec)
            return httpFail(ec, "read");

        auto ip = derived().ip();

        if (!ip)
        {
            return;
        }

        auto const httpResponse = [&](http::status status, std::string content_type, std::string message) {
            http::response<http::string_body> res{status, req_.version()};
            res.set(http::field::server, "clio-server-" + Build::getClioVersionString());
            res.set(http::field::content_type, content_type);
            res.keep_alive(req_.keep_alive());
            res.body() = std::string(message);
            res.prepare_payload();
            return res;
        };

        if (boost::beast::websocket::is_upgrade(req_))
        {
            upgraded_ = true;
            // Disable the timeout.
            // The websocket::stream uses its own timeout settings.
            boost::beast::get_lowest_layer(derived().stream()).expires_never();
            return make_WebsocketSession(
                ioc_,
                derived().releaseStream(),
                derived().ip(),
                std::move(req_),
                std::move(buffer_),
                backend_,
                rpcEngine_,
                subscriptions_,
                balancer_,
                etl_,
                tagFactory_,
                dosGuard_);
        }

        // to avoid overwhelm work queue, the request limit check should be
        // before posting to queue the web socket creation will be guarded via
        // connection limit
        if (!dosGuard_.request(ip.value()))
        {
            return lambda_(httpResponse(http::status::service_unavailable, "text/plain", "Server is overloaded"));
        }

        log_.info() << tag() << "Received request from ip = " << *ip << " - posting to WorkQueue";

        auto session = derived().shared_from_this();

        if (not rpcEngine_->post(
                [this, ip, session](boost::asio::yield_context yield) {
                    handleRequest(
                        yield,
                        std::move(req_),
                        lambda_,
                        backend_,
                        rpcEngine_,
                        subscriptions_,
                        balancer_,
                        etl_,
                        tagFactory_,
                        dosGuard_,
                        *ip,
                        session,
                        perfLog_);
                },
                ip.value()))
        {
            // Non-whitelist connection rejected due to full connection
            // queue
            lambda_(httpResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcTOO_BUSY))));
        }
    }

    void
    onWrite(bool close, boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return httpFail(ec, "write");

        if (close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return derived().doClose();
        }

        // We're done with the response so delete it
        res_ = nullptr;

        // Read another request
        doRead();
    }
};

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <class Body, class Allocator, class Send, class Session>
void
handleRequest(
    boost::asio::yield_context& yc,
    boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>&& req,
    Send&& send,
    std::shared_ptr<BackendInterface const> backend,
    std::shared_ptr<RPC::RPCEngine> rpcEngine,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<ETLLoadBalancer> balancer,
    std::shared_ptr<ReportingETL const> etl,
    util::TagDecoratorFactory const& tagFactory,
    clio::DOSGuard& dosGuard,
    std::string const& ip,
    std::shared_ptr<Session> http,
    clio::Logger& perfLog)
{
    auto const httpResponse = [&req](http::status status, std::string content_type, std::string message) {
        http::response<http::string_body> res{status, req.version()};
        res.set(http::field::server, "clio-server-" + Build::getClioVersionString());
        res.set(http::field::content_type, content_type);
        res.keep_alive(req.keep_alive());
        res.body() = std::string(message);
        res.prepare_payload();
        return res;
    };

    if (req.method() == http::verb::get && req.body() == "")
    {
        send(httpResponse(http::status::ok, "text/html", defaultResponse));
        return;
    }

    if (req.method() != http::verb::post)
        return send(httpResponse(http::status::bad_request, "text/html", "Expected a POST request"));

    try
    {
        perfLog.debug() << http->tag() << "http received request from work queue: " << req.body();

        boost::json::object request;
        std::string responseStr = "";
        try
        {
            request = boost::json::parse(req.body()).as_object();

            if (!request.contains("params"))
                request["params"] = boost::json::array({boost::json::object{}});
        }
        catch (std::runtime_error const& e)
        {
            return send(httpResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcBAD_SYNTAX))));
        }

        auto range = backend->fetchLedgerRange();
        if (!range)
            return send(httpResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcNOT_READY))));

        auto context = RPC::make_HttpContext(yc, request, tagFactory.with(std::cref(http->tag())), *range, ip);

        if (!context)
            return send(httpResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcBAD_SYNTAX))));

        boost::json::object response;
        auto [v, timeDiff] = util::timed([&]() { return rpcEngine->buildResponse(*context); });

        auto us = std::chrono::duration<int, std::milli>(timeDiff);
        RPC::logDuration(*context, us);

        if (auto status = std::get_if<RPC::Status>(&v))
        {
            rpcEngine->notifyErrored(context->method);
            auto error = RPC::makeError(*status);
            error["request"] = request;
            response["result"] = error;

            perfLog.debug() << http->tag() << "Encountered error: " << responseStr;
        }
        else
        {
            // This can still technically be an error. Clio counts forwarded
            // requests as successful.

            rpcEngine->notifyComplete(context->method, us);

            auto result = std::get<boost::json::object>(v);
            if (result.contains("result") && result.at("result").is_object())
                result = result.at("result").as_object();

            if (!result.contains("error"))
                result["status"] = "success";

            response["result"] = result;
        }

        boost::json::array warnings;
        warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_CLIO));
        auto lastCloseAge = etl->lastCloseAgeSeconds();
        if (lastCloseAge >= 60)
            warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_OUTDATED));
        response["warnings"] = warnings;
        responseStr = boost::json::serialize(response);
        if (!dosGuard.add(ip, responseStr.size()))
        {
            response["warning"] = "load";
            warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_RATE_LIMIT));
            response["warnings"] = warnings;
            // reserialize when we need to include this warning
            responseStr = boost::json::serialize(response);
        }
        return send(httpResponse(http::status::ok, "application/json", responseStr));
    }
    catch (std::exception const& e)
    {
        perfLog.error() << http->tag() << "Caught exception : " << e.what();
        return send(httpResponse(
            http::status::internal_server_error,
            "application/json",
            boost::json::serialize(RPC::makeError(RPC::RippledError::rpcINTERNAL))));
    }
}
