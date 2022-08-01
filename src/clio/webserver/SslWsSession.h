#ifndef RIPPLE_REPORTING_SSL_WS_SESSION_H
#define RIPPLE_REPORTING_SSL_WS_SESSION_H

#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <clio/etl/ReportingETL.h>

#include <clio/webserver/WsBase.h>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

class ReportingETL;

class SslWsSession : public WsSession<SslWsSession>
{
    boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>>
        ws_;

public:
    // Take ownership of the socket
    explicit SslWsSession(
        Application const& app,
        boost::beast::ssl_stream<boost::beast::tcp_stream>&& stream,
        boost::beast::flat_buffer&& b)
        : WsSession(app, std::move(b)), ws_(std::move(stream))
    {
    }
    boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>>&
    ws()
    {
        return ws_;
    }

    std::optional<std::string>
    ip()
    {
        try
        {
            return ws()
                .next_layer()
                .next_layer()
                .socket()
                .remote_endpoint()
                .address()
                .to_string();
        }
        catch (std::exception const&)
        {
            return {};
        }
    }
};

class SslWsUpgrader : public std::enable_shared_from_this<SslWsUpgrader>
{
    Application const& app_;
    boost::beast::ssl_stream<boost::beast::tcp_stream> https_;
    boost::optional<http::request_parser<http::string_body>> parser_;
    boost::beast::flat_buffer buffer_;
    http::request<http::string_body> req_;

public:
    SslWsUpgrader(
        Application const& app,
        boost::asio::ip::tcp::socket&& socket,
        boost::beast::flat_buffer&& b)
        : app_(app)
        , https_(std::move(socket), *app.sslContext())
        , buffer_(std::move(b))
    {
    }

    SslWsUpgrader(
        Application const& app,
        boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
        boost::beast::flat_buffer&& b,
        http::request<http::string_body> req)
        : app_(app)
        , https_(std::move(stream))
        , buffer_(std::move(b))
        , req_(std::move(req))
    {
    }

    ~SslWsUpgrader() = default;

    void
    run()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(https_).expires_after(
            std::chrono::seconds(30));

        net::dispatch(
            https_.get_executor(),
            boost::beast::bind_front_handler(
                &SslWsUpgrader::do_upgrade, shared_from_this()));
    }

private:
    void
    on_handshake(boost::beast::error_code ec, std::size_t bytes_used)
    {
        if (ec)
            return logError(ec, "handshake");

        // Consume the portion of the buffer used by the handshake
        buffer_.consume(bytes_used);

        do_upgrade();
    }

    void
    do_upgrade()
    {
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser_->body_limit(10000);

        // Set the timeout.
        boost::beast::get_lowest_layer(https_).expires_after(
            std::chrono::seconds(30));

        on_upgrade();
    }

    void
    on_upgrade()
    {
        // See if it is a WebSocket Upgrade
        if (!websocket::is_upgrade(req_))
        {
            return;
        }

        // Disable the timeout.
        // The websocket::stream uses its own timeout settings.
        boost::beast::get_lowest_layer(https_).expires_never();

        std::make_shared<SslWsSession>(
            app_, std::move(https_), std::move(buffer_))
            ->run(std::move(req_));
    }
};

#endif  // RIPPLE_REPORTING_SSL_WS_SESSION_H
