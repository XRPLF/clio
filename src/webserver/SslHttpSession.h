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

#include <webserver/SslWsSession.h>
#include <webserver/impl/HttpBase.h>

namespace web {

using tcp = boost::asio::ip::tcp;

/**
 * @brief A session for handling HTTP requests over SSL.
 * It will handle the SSL handshake and then pass control to the base class.
 * It will also handle the session upgrade to WebSocket.
 */
template <ServerHandler Handler>
class SslHttpSession : public detail::HttpBase<SslHttpSession, Handler>,
                       public std::enable_shared_from_this<SslHttpSession<Handler>>
{
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
    std::reference_wrapper<clio::util::TagDecoratorFactory const> tagFactory_;

public:
    explicit SslHttpSession(
        tcp::socket&& socket,
        std::string const& ip,
        boost::asio::ssl::context& ctx,
        std::reference_wrapper<clio::util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<web::DOSGuard> dosGuard,
        std::shared_ptr<Handler> const& handler,
        boost::beast::flat_buffer buffer)
        : detail::HttpBase<SslHttpSession, Handler>(ip, tagFactory, dosGuard, handler, std::move(buffer))
        , stream_(std::move(socket), ctx)
        , tagFactory_(tagFactory)
    {
    }

    ~SslHttpSession() = default;

    boost::beast::ssl_stream<boost::beast::tcp_stream>&
    stream()
    {
        return stream_;
    }

    void
    run()
    {
        auto self = this->shared_from_this();
        boost::asio::dispatch(stream_.get_executor(), [self]() {
            // Set the timeout.
            boost::beast::get_lowest_layer(self->stream()).expires_after(std::chrono::seconds(30));

            // Perform the SSL handshake
            // Note, this is the buffered version of the handshake.
            self->stream_.async_handshake(
                boost::asio::ssl::stream_base::server,
                self->buffer_.data(),
                boost::beast::bind_front_handler(&SslHttpSession<Handler>::onHandshake, self));
        });
    }

    void
    onHandshake(boost::beast::error_code ec, std::size_t bytes_used)
    {
        if (ec)
            return this->httpFail(ec, "handshake");

        this->buffer_.consume(bytes_used);

        this->doRead();
    }

    void
    doClose()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
        // Perform the SSL shutdown
        stream_.async_shutdown(boost::beast::bind_front_handler(&SslHttpSession::onShutdown, this->shared_from_this()));
    }

    void
    onShutdown(boost::beast::error_code ec)
    {
        if (ec)
            return this->httpFail(ec, "shutdown");
        // At this point the connection is closed gracefully
    }

    void
    upgrade()
    {
        std::make_shared<SslWsUpgrader<Handler>>(
            std::move(stream_),
            this->clientIp,
            tagFactory_,
            this->dosGuard_,
            this->handler_,
            std::move(this->buffer_),
            std::move(this->req_))
            ->run();
    }
};
}  // namespace web
