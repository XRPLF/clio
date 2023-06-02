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

#include <webserver/HttpBase.h>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

// Handles an HTTPS server connection
class SslHttpSession : public HttpBase<SslHttpSession>, public std::enable_shared_from_this<SslHttpSession>
{
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
    std::optional<std::string> ip_;

public:
    // Take ownership of the socket
    explicit SslHttpSession(
        boost::asio::io_context& ioc,
        tcp::socket&& socket,
        ssl::context& ctx,
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<RPC::RPCEngine> rpcEngine,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<LoadBalancer> balancer,
        std::shared_ptr<ETLService const> etl,
        util::TagDecoratorFactory const& tagFactory,
        clio::DOSGuard& dosGuard,
        boost::beast::flat_buffer buffer)
        : HttpBase<SslHttpSession>(
              ioc,
              backend,
              rpcEngine,
              subscriptions,
              balancer,
              etl,
              tagFactory,
              dosGuard,
              std::move(buffer))
        , stream_(std::move(socket), ctx)
    {
        try
        {
            ip_ = stream_.next_layer().socket().remote_endpoint().address().to_string();
        }
        catch (std::exception const&)
        {
        }
        if (ip_)
            HttpBase::dosGuard().increment(*ip_);
    }

    ~SslHttpSession()
    {
        if (ip_ and not upgraded_)
            HttpBase::dosGuard().decrement(*ip_);
    }

    boost::beast::ssl_stream<boost::beast::tcp_stream>&
    stream()
    {
        return stream_;
    }
    boost::beast::ssl_stream<boost::beast::tcp_stream>
    releaseStream()
    {
        return std::move(stream_);
    }

    std::optional<std::string>
    ip()
    {
        return ip_;
    }

    // Start the asynchronous operation
    void
    run()
    {
        auto self = shared_from_this();
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session.
        net::dispatch(stream_.get_executor(), [self]() {
            // Set the timeout.
            boost::beast::get_lowest_layer(self->stream()).expires_after(std::chrono::seconds(30));

            // Perform the SSL handshake
            // Note, this is the buffered version of the handshake.
            self->stream_.async_handshake(
                ssl::stream_base::server,
                self->buffer_.data(),
                boost::beast::bind_front_handler(&SslHttpSession::onHandshake, self));
        });
    }

    void
    onHandshake(boost::beast::error_code ec, std::size_t bytes_used)
    {
        if (ec)
            return httpFail(ec, "handshake");

        buffer_.consume(bytes_used);

        doRead();
    }

    void
    doClose()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Perform the SSL shutdown
        stream_.async_shutdown(boost::beast::bind_front_handler(&SslHttpSession::onShutdown, shared_from_this()));
    }

    void
    onShutdown(boost::beast::error_code ec)
    {
        if (ec)
            return httpFail(ec, "shutdown");

        // At this point the connection is closed gracefully
    }
};
