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

#include "util/Taggable.h"
#include "web/DOSGuard.h"
#include "web/impl/WsBase.h"
#include "web/interface/ConnectionBase.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/optional/optional.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace web {

/**
 * @brief Represents a non-secure websocket session.
 *
 * Majority of the operations are handled by the base class.
 */
template <SomeServerHandler HandlerType>
class PlainWsSession : public detail::WsBase<PlainWsSession, HandlerType> {
    using StreamType = boost::beast::websocket::stream<boost::beast::tcp_stream>;
    StreamType ws_;

public:
    /**
     * @brief Create a new non-secure websocket session.
     *
     * @param socket The socket. Ownership is transferred
     * @param ip Client's IP address
     * @param tagFactory A factory that is used to generate tags to track requests and sessions
     * @param dosGuard The denial of service guard to use
     * @param handler The server handler to use
     * @param buffer Buffer with initial data received from the peer
     * @param isAdmin Whether the connection has admin privileges
     */
    explicit PlainWsSession(
        boost::asio::ip::tcp::socket&& socket,
        std::string ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<web::DOSGuard> dosGuard,
        std::shared_ptr<HandlerType> const& handler,
        boost::beast::flat_buffer&& buffer,
        bool isAdmin
    )
        : detail::WsBase<PlainWsSession, HandlerType>(ip, tagFactory, dosGuard, handler, std::move(buffer))
        , ws_(std::move(socket))
    {
        ConnectionBase::isAdmin_ = isAdmin;  // NOLINT(cppcoreguidelines-prefer-member-initializer)
    }

    ~PlainWsSession() override = default;

    /** @return The websocket stream. */
    StreamType&
    ws()
    {
        return ws_;
    }
};

/**
 * @brief The websocket upgrader class, upgrade from an HTTP session to a non-secure websocket session.
 *
 * Pass the socket to the session class after upgrade.
 */
template <SomeServerHandler HandlerType>
class WsUpgrader : public std::enable_shared_from_this<WsUpgrader<HandlerType>> {
    using std::enable_shared_from_this<WsUpgrader<HandlerType>>::shared_from_this;

    boost::beast::tcp_stream http_;
    boost::optional<http::request_parser<http::string_body>> parser_;
    boost::beast::flat_buffer buffer_;
    std::reference_wrapper<util::TagDecoratorFactory const> tagFactory_;
    std::reference_wrapper<web::DOSGuard> dosGuard_;
    http::request<http::string_body> req_;
    std::string ip_;
    std::shared_ptr<HandlerType> const handler_;
    bool isAdmin_;

public:
    /**
     * @brief Create a new upgrader to non-secure websocket.
     *
     * @param stream The TCP stream. Ownership is transferred
     * @param ip Client's IP address
     * @param tagFactory A factory that is used to generate tags to track requests and sessions
     * @param dosGuard The denial of service guard to use
     * @param handler The server handler to use
     * @param buffer Buffer with initial data received from the peer. Ownership is transferred
     * @param request The request. Ownership is transferred
     * @param isAdmin Whether the connection has admin privileges
     */
    WsUpgrader(
        boost::beast::tcp_stream&& stream,
        std::string ip,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<web::DOSGuard> dosGuard,
        std::shared_ptr<HandlerType> const& handler,
        boost::beast::flat_buffer&& buffer,
        http::request<http::string_body> request,
        bool isAdmin
    )
        : http_(std::move(stream))
        , buffer_(std::move(buffer))
        , tagFactory_(tagFactory)
        , dosGuard_(dosGuard)
        , req_(std::move(request))
        , ip_(std::move(ip))
        , handler_(handler)
        , isAdmin_(isAdmin)
    {
    }

    /** @brief Initiate the upgrade. */
    void
    run()
    {
        boost::asio::dispatch(
            http_.get_executor(),
            boost::beast::bind_front_handler(&WsUpgrader<HandlerType>::doUpgrade, shared_from_this())
        );
    }

private:
    void
    doUpgrade()
    {
        parser_.emplace();

        constexpr static auto maxBodySize = 10000;
        parser_->body_limit(maxBodySize);

        boost::beast::get_lowest_layer(http_).expires_after(std::chrono::seconds(30));
        onUpgrade();
    }

    void
    onUpgrade()
    {
        if (!boost::beast::websocket::is_upgrade(req_))
            return;

        // Disable the timeout. The websocket::stream uses its own timeout settings.
        boost::beast::get_lowest_layer(http_).expires_never();

        std::make_shared<PlainWsSession<HandlerType>>(
            http_.release_socket(), ip_, tagFactory_, dosGuard_, handler_, std::move(buffer_), isAdmin_
        )
            ->run(std::move(req_));
    }
};

}  // namespace web
