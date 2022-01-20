#ifndef RIPPLE_REPORTING_SSL_WS_SESSION_H
#define RIPPLE_REPORTING_SSL_WS_SESSION_H

#include <boost/asio/dispatch.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <etl/ReportingETL.h>

#include <webserver/WsBase.h>

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
        boost::beast::ssl_stream<boost::beast::tcp_stream>&& stream,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard,
        RPC::Counters& counters,
        boost::beast::flat_buffer&& b)
        : WsSession(
              backend,
              subscriptions,
              balancer,
              dosGuard,
              counters,
              std::move(b))
        , ws_(std::move(stream))
    {
    }
    boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>>&
    ws()
    {
        return ws_;
    }
    std::string
    ip()
    {
        return ws()
            .next_layer()
            .next_layer()
            .socket()
            .remote_endpoint()
            .address()
            .to_string();
    }
};

class SslWsUpgrader : public std::enable_shared_from_this<SslWsUpgrader>
{
    boost::beast::ssl_stream<boost::beast::tcp_stream> https_;
    boost::optional<http::request_parser<http::string_body>> parser_;
    boost::beast::flat_buffer buffer_;
    std::shared_ptr<BackendInterface const> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    DOSGuard& dosGuard_;
    RPC::Counters& counters_;
    http::request<http::string_body> req_;

public:
    SslWsUpgrader(
        boost::asio::ip::tcp::socket&& socket,
        ssl::context& ctx,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard,
        RPC::Counters& counters,
        boost::beast::flat_buffer&& b)
        : https_(std::move(socket), ctx)
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
        , counters_(counters)
        , buffer_(std::move(b))
    {
    }
    SslWsUpgrader(
        boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        DOSGuard& dosGuard,
        RPC::Counters& counters,
        boost::beast::flat_buffer&& b,
        http::request<http::string_body> req)
        : https_(std::move(stream))
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , dosGuard_(dosGuard)
        , counters_(counters)
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
            std::move(https_),
            backend_,
            subscriptions_,
            balancer_,
            dosGuard_,
            counters_,
            std::move(buffer_))
            ->run(std::move(req_));
    }
};

#endif  // RIPPLE_REPORTING_SSL_WS_SESSION_H
