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

#include "util/Taggable.hpp"
#include "util/config/Config.hpp"
#include "web/SslWsSession.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"
#include "web/impl/HttpBase.hpp"
#include "web/interface/Concepts.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace web {

using tcp = boost::asio::ip::tcp;

/**
 * @brief Represents a HTTPS connection established by a client.
 *
 * It will handle the upgrade to secure websocket, pass the ownership of the socket to the upgrade session.
 * Otherwise, it will pass control to the base class.
 *
 * @tparam HandlerType The type of the server handler to use
 */
template <SomeServerHandler HandlerType>
class SslHttpSession : public impl::HttpBase<SslHttpSession, HandlerType>,
                       public std::enable_shared_from_this<SslHttpSession<HandlerType>> {
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
    util::Config config_;
    std::reference_wrapper<util::TagDecoratorFactory const> tagFactory_;

public:
    /**
     * @brief Create a new SSL session.
     *
     * @param socket The socket. Ownership is transferred to HttpSession
     * @param config The config for server
     * @param ip Client's IP address
     * @param adminVerification The admin verification strategy to use
     * @param ctx The SSL context
     * @param tagFactory A factory that is used to generate tags to track requests and sessions
     * @param dosGuard The denial of service guard to use
     * @param handler The server handler to use
     * @param buffer Buffer with initial data received from the peer
     */
    explicit SslHttpSession(
        tcp::socket&& socket,
        util::Config config,
        std::string const& ip,
        std::shared_ptr<impl::AdminVerificationStrategy> const& adminVerification,
        boost::asio::ssl::context& ctx,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<dosguard::DOSGuardInterface> dosGuard,
        std::shared_ptr<HandlerType> const& handler,
        boost::beast::flat_buffer buffer
    )
        : impl::HttpBase<SslHttpSession, HandlerType>(
              ip,
              tagFactory,
              adminVerification,
              dosGuard,
              handler,
              std::move(buffer)
          )
        , stream_(std::move(socket), ctx)
        , config_(std::move(config))
        , tagFactory_(tagFactory)
    {
    }

    ~SslHttpSession() override = default;

    /** @return The SSL stream. */
    boost::beast::ssl_stream<boost::beast::tcp_stream>&
    stream()
    {
        return stream_;
    }

    /** @brief Initiates the handshake. */
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
                boost::beast::bind_front_handler(&SslHttpSession<HandlerType>::onHandshake, self)
            );
        });
    }

    /**
     * @brief Handles the handshake.
     *
     * @param ec Error code if any
     * @param bytesUsed The total amount of data read from the stream
     */
    void
    onHandshake(boost::beast::error_code ec, std::size_t bytesUsed)
    {
        if (ec)
            return this->httpFail(ec, "handshake");

        this->buffer_.consume(bytesUsed);
        this->doRead();
    }

    /** @brief Closes the underlying connection. */
    void
    doClose()
    {
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
        stream_.async_shutdown(boost::beast::bind_front_handler(&SslHttpSession::onShutdown, this->shared_from_this()));
    }

    /**
     * @brief Handles a connection shutdown.
     *
     * @param ec Error code if any
     */
    void
    onShutdown(boost::beast::error_code ec)
    {
        if (ec)
            return this->httpFail(ec, "shutdown");
        // At this point the connection is closed gracefully
    }

    /** @brief Upgrades connection to secure websocket. */
    void
    upgrade()
    {
        std::make_shared<SslWsUpgrader<HandlerType>>(
            std::move(stream_),
            config_,
            this->clientIp,
            tagFactory_,
            this->dosGuard_,
            this->handler_,
            std::move(this->buffer_),
            std::move(this->req_),
            ConnectionBase::isAdmin()
        )
            ->run();
    }
};
}  // namespace web
