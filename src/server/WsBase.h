#ifndef RIPPLE_REPORTING_WS_BASE_SESSION_H
#define RIPPLE_REPORTING_WS_BASE_SESSION_H

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <backend/BackendInterface.h>
#include <etl/ETLSource.h>
#include <iostream>
#include <memory>
#include <server/DOSGuard.h>
#include <server/SubscriptionManager.h>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

inline void
wsFail(boost::beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

class WsBase
{
public:
    // Send, that enables SubscriptionManager to publish to clients
    virtual void
    send(std::string&& msg) = 0;

    virtual ~WsBase()
    {
    }
};

// Echoes back all received WebSocket messages
template <class Derived>
class WsSession : public WsBase,
                  public std::enable_shared_from_this<WsSession<Derived>>
{
    boost::beast::flat_buffer buffer_;
    std::string response_;

    std::shared_ptr<BackendInterface> backend_;
    std::weak_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    DOSGuard& dosGuard_;

public:
    explicit WsSession(
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard,
        boost::beast::flat_buffer&& buffer)
        : backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
        , buffer_(std::move(buffer))
    {
    }
    virtual ~WsSession()
    {
    }
    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }
    void
    send(std::string&& msg)
    {
        derived().ws().text(derived().ws().got_text());
        derived().ws().async_write(
            boost::asio::buffer(msg),
            boost::beast::bind_front_handler(
                &WsSession::on_write, this->shared_from_this()));
    }
    void
    run(http::request<http::string_body> req)
    {
        std::cout << "Running ws" << std::endl;
        // Set suggested timeout settings for the websocket
        derived().ws().set_option(websocket::stream_base::timeout::suggested(
            boost::beast::role_type::server));

        std::cout << "Trying to decorate" << std::endl;
        // Set a decorator to change the Server of the handshake
        derived().ws().set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(
                    http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-server-async");
            }));

        std::cout << "trying to async accept" << std::endl;

        derived().ws().async_accept(
            req,
            boost::beast::bind_front_handler(
                &WsSession::on_accept, this->shared_from_this()));
    }

    void
    on_accept(boost::beast::error_code ec)
    {
        if (ec)
            return wsFail(ec, "accept");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        // Read a message into our buffer
        derived().ws().async_read(
            buffer_,
            boost::beast::bind_front_handler(
                &WsSession::on_read, this->shared_from_this()));
    }

    void
    on_read(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed
        if (ec == boost::beast::websocket::error::closed)
        {
            std::cout << "session closed" << std::endl;
            return;
        }

        if (ec)
            return wsFail(ec, "read");

        std::string msg{
            static_cast<char const*>(buffer_.data().data()), buffer_.size()};
        // BOOST_LOG_TRIVIAL(debug) << __func__ << msg;
        boost::json::object response;
        auto ip = derived().ip();
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
                        this->shared_from_this());
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
        derived().ws().text(derived().ws().got_text());
        derived().ws().async_write(
            boost::asio::buffer(response_),
            boost::beast::bind_front_handler(
                &WsSession::on_write, this->shared_from_this()));
    }

    void
    on_write(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return wsFail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};
#endif  // RIPPLE_REPORTING_WS_BASE_SESSION_H
