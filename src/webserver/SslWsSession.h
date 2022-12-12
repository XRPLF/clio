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
        boost::asio::io_context& ioc,
        boost::beast::ssl_stream<boost::beast::tcp_stream>&& stream,
        std::optional<std::string> ip,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        std::shared_ptr<ReportingETL const> etl,
        util::TagDecoratorFactory const& tagFactory,
        DOSGuard& dosGuard,
        RPC::Counters& counters,
        WorkQueue& queue,
        boost::beast::flat_buffer&& b)
        : WsSession(
              ioc,
              ip,
              backend,
              subscriptions,
              balancer,
              etl,
              tagFactory,
              dosGuard,
              counters,
              queue,
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

    std::optional<std::string>
    ip()
    {
        return ip_;
    }
};

class SslWsUpgrader : public std::enable_shared_from_this<SslWsUpgrader>
{
    boost::asio::io_context& ioc_;
    boost::beast::ssl_stream<boost::beast::tcp_stream> https_;
    boost::optional<http::request_parser<http::string_body>> parser_;
    boost::beast::flat_buffer buffer_;
    std::optional<std::string> ip_;
    std::shared_ptr<BackendInterface const> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> balancer_;
    std::shared_ptr<ReportingETL const> etl_;
    util::TagDecoratorFactory const& tagFactory_;
    DOSGuard& dosGuard_;
    RPC::Counters& counters_;
    WorkQueue& queue_;
    http::request<http::string_body> req_;

public:
    SslWsUpgrader(
        boost::asio::io_context& ioc,
        std::optional<std::string> ip,
        boost::asio::ip::tcp::socket&& socket,
        ssl::context& ctx,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        std::shared_ptr<ReportingETL const> etl,
        util::TagDecoratorFactory const& tagFactory,
        DOSGuard& dosGuard,
        RPC::Counters& counters,
        WorkQueue& queue,
        boost::beast::flat_buffer&& b)
        : ioc_(ioc)
        , https_(std::move(socket), ctx)
        , buffer_(std::move(b))
        , ip_(ip)
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , etl_(etl)
        , tagFactory_(tagFactory)
        , dosGuard_(dosGuard)
        , counters_(counters)
        , queue_(queue)
    {
    }
    SslWsUpgrader(
        boost::asio::io_context& ioc,
        boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
        std::optional<std::string> ip,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        std::shared_ptr<ReportingETL const> etl,
        util::TagDecoratorFactory const& tagFactory,
        DOSGuard& dosGuard,
        RPC::Counters& counters,
        WorkQueue& queue,
        boost::beast::flat_buffer&& b,
        http::request<http::string_body> req)
        : ioc_(ioc)
        , https_(std::move(stream))
        , buffer_(std::move(b))
        , ip_(ip)
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , etl_(etl)
        , tagFactory_(tagFactory)
        , dosGuard_(dosGuard)
        , counters_(counters)
        , queue_(queue)
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
            ioc_,
            std::move(https_),
            ip_,
            backend_,
            subscriptions_,
            balancer_,
            etl_,
            tagFactory_,
            dosGuard_,
            counters_,
            queue_,
            std::move(buffer_))
            ->run(std::move(req_));
    }
};
