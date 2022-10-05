#ifndef RIPPLE_REPORTING_HTTP_BASE_SESSION_H
#define RIPPLE_REPORTING_HTTP_BASE_SESSION_H

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

#include <rpc/Counters.h>
#include <rpc/RPC.h>
#include <rpc/WorkQueue.h>
#include <vector>
#include <webserver/DOSGuard.h>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

static std::string defaultResponse =
    "<!DOCTYPE html><html><head><title>"
    " Test page for clio</title></head><body><h1>"
    " Test</h1><p>This page shows clio http(s) "
    "connectivity is working.</p></body></html>";

http::response<http::string_body>
makeHTTPResponse(
    http::status const& status,
    std::string const& content_type,
    std::string const& message,
    unsigned version,
    bool keepAlive)
{
    http::response<http::string_body> res{status, version};
    res.set(
        http::field::server, "clio-server-" + Build::getClioVersionString());
    res.set(http::field::content_type, content_type);
    res.keep_alive(keepAlive);
    res.body() = std::string(message);
    res.prepare_payload();
    return res;
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
            if (self_.dead())
                return;

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

    boost::system::error_code ec_;
    boost::asio::io_context& ioc_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;
    std::shared_ptr<BackendInterface const> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    std::shared_ptr<ReportingETL const> etl_;
    DOSGuard& dosGuard_;
    RPC::Counters& counters_;
    WorkQueue& workQueue_;
    send_lambda lambda_;

protected:
    boost::beast::flat_buffer buffer_;

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
            BOOST_LOG_TRIVIAL(info)
                << "httpFail: " << what << ": " << ec.message();
            boost::beast::get_lowest_layer(derived().stream())
                .socket()
                .close(ec);
        }
    }

public:
    HttpBase(
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        std::shared_ptr<ReportingETL const> etl,
        DOSGuard& dosGuard,
        RPC::Counters& counters,
        WorkQueue& queue,
        boost::beast::flat_buffer buffer)
        : ioc_(ioc)
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , etl_(etl)
        , dosGuard_(dosGuard)
        , counters_(counters)
        , workQueue_(queue)
        , lambda_(*this)
        , buffer_(std::move(buffer))
    {
    }

    void
    do_read()
    {
        if (dead())
            return;
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
                ioc_,
                derived().release_stream(),
                std::move(req_),
                std::move(buffer_),
                backend_,
                subscriptions_,
                balancer_,
                etl_,
                dosGuard_,
                counters_,
                workQueue_);
        }

        auto ip = derived().ip();

        if (!ip)
            return;

        auto session = derived().shared_from_this();
        // The DOS guard allocates tickets to concurrent requests.
        // The resource is automatically checked in when ticket goes out of
        // scope.
        auto ticket = dosGuard_.checkout(*ip);
        if (!ticket.isValid())
            return lambda_(makeHTTPResponse(
                http::status::service_unavailable,
                "text/plain",
                "Server is overloaded",
                req_.version(),
                req_.keep_alive()));

        // Requests are handed using coroutines. Here we spawn a coroutine
        // which will asynchronously handle a request.
        if (!workQueue_.postCoro(
                [this, ip, session, t = std::move(ticket)](
                    boost::asio::yield_context yield) {
                    handle_request(
                        yield,
                        std::move(req_),
                        lambda_,
                        backend_,
                        subscriptions_,
                        balancer_,
                        etl_,
                        dosGuard_,
                        counters_,
                        *ip,
                        session);
                },
                dosGuard_.isWhiteListed(*ip)))
        {
            lambda_(makeHTTPResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(
                    RPC::make_error(RPC::Error::rpcTOO_BUSY)),
                req_.version(),
                req_.keep_alive()));
        }
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

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template <class Body, class Allocator, class Send, class Session>
void
handle_request(
    boost::asio::yield_context& yc,
    boost::beast::http::
        request<Body, boost::beast::http::basic_fields<Allocator>>&& req,
    Send&& send,
    std::shared_ptr<BackendInterface const> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<ETLLoadBalancer> balancer,
    std::shared_ptr<ReportingETL const> etl,
    DOSGuard& dosGuard,
    RPC::Counters& counters,
    std::string const& ip,
    std::shared_ptr<Session> http)
{
    if (req.method() == http::verb::get && req.body() == "")
        return send(makeHTTPResponse(
            http::status::ok,
            "text/html",
            defaultResponse,
            req.version(),
            req.keep_alive()));

    if (req.method() != http::verb::post)
        return send(makeHTTPResponse(
            http::status::bad_request,
            "text/html",
            "Expected a POST request",
            req.version(),
            req.keep_alive()));

    try
    {
        BOOST_LOG_TRIVIAL(debug) << "Received request: " << req.body();

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
            return send(makeHTTPResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(
                    RPC::make_error(RPC::Error::rpcBAD_SYNTAX)),
                req.version(),
                req.keep_alive()));
        }

        auto range = backend->fetchLedgerRange();
        if (!range)
            return send(makeHTTPResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(
                    RPC::make_error(RPC::Error::rpcNOT_READY)),
                req.version(),
                req.keep_alive()));

        std::optional<RPC::Context> context = RPC::make_HttpContext(
            yc,
            request,
            backend,
            subscriptions,
            balancer,
            etl,
            *range,
            counters,
            ip);

        if (!context)
            return send(makeHTTPResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(
                    RPC::make_error(RPC::Error::rpcBAD_SYNTAX)),
                req.version(),
                req.keep_alive()));

        boost::json::object response{{"result", boost::json::object{}}};
        boost::json::object& result = response["result"].as_object();

        auto start = std::chrono::system_clock::now();
        auto v = RPC::buildResponse(*context);
        auto end = std::chrono::system_clock::now();
        auto us =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        RPC::logDuration(*context, us);

        if (auto status = std::get_if<RPC::Status>(&v))
        {
            counters.rpcErrored(context->method);
            auto error = RPC::make_error(*status);

            error["request"] = request;

            result = error;

            BOOST_LOG_TRIVIAL(debug)
                << __func__ << " Encountered error: " << responseStr;
        }
        else
        {
            // This can still technically be an error. Clio counts forwarded
            // requests as successful.

            counters.rpcComplete(context->method, us);
            result = std::get<boost::json::object>(v);

            if (!result.contains("error"))
                result["status"] = "success";
        }

        boost::json::array warnings;
        warnings.emplace_back(RPC::make_warning(RPC::warnRPC_CLIO));
        auto lastCloseAge = context->etl->lastCloseAgeSeconds();
        if (lastCloseAge >= 60)
            warnings.emplace_back(RPC::make_warning(RPC::warnRPC_OUTDATED));
        response["warnings"] = warnings;
        responseStr = boost::json::serialize(response);
        if (!dosGuard.add(ip, responseStr.size()))
        {
            response["warning"] = "load";
            warnings.emplace_back(RPC::make_warning(RPC::warnRPC_RATE_LIMIT));
            response["warnings"] = warnings;
            // reserialize when we need to include this warning
            responseStr = boost::json::serialize(response);
        }
        return send(makeHTTPResponse(
            http::status::ok,
            "application/json",
            responseStr,
            req.version(),
            req.keep_alive()));
    }
    catch (std::exception const& e)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__ << " Caught exception : " << e.what();
        return send(makeHTTPResponse(
            http::status::internal_server_error,
            "application/json",
            boost::json::serialize(RPC::make_error(RPC::Error::rpcINTERNAL)),
            req.version(),
            req.keep_alive()));
    }
}

#endif  // RIPPLE_REPORTING_HTTP_BASE_SESSION_H
